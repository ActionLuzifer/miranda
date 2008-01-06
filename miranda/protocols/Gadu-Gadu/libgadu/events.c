/* coding: UTF-8 */
/* $Id$ */

/*
 *  (C) Copyright 2001-2006 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Woźny <speedy@ziew.org>
 *                          Arkadiusz Miśkiewicz <arekm@pld-linux.org>
 *                          Adam Wysocki <gophi@ekg.chmurka.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

/**
 * \file events.c
 *
 * \brief Obsługa zdarzeń
 */

#include "libgadu-config.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#ifdef GG_CONFIG_HAVE_PTHREAD
#  include <pthread.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#ifndef GG_CONFIG_MIRANDA
#ifdef GG_CONFIG_HAVE_OPENSSL
#  include <openssl/err.h>
#  include <openssl/x509.h>
#endif
#endif

#include "compat.h"
#include "libgadu.h"

/**
 * Zwalnia pamięć zajmowaną przez informację o zdarzeniu.
 *
 * Funkcję należy wywoływać za każdym razem gdy funkcja biblioteki zwróci
 * strukturę \c gg_event.
 *
 * \param e Struktura zdarzenia
 */
void gg_event_free(struct gg_event *e)
{
	gg_debug(GG_DEBUG_FUNCTION, "** gg_event_free(%p);\n", e);

	if (!e)
		return;

	switch (e->type) {
		case GG_EVENT_MSG:
			free(e->event.msg.message);
			free(e->event.msg.formats);
			free(e->event.msg.recipients);
			break;

		case GG_EVENT_NOTIFY:
			free(e->event.notify);
			break;

		case GG_EVENT_NOTIFY60:
		{
			int i;

			for (i = 0; e->event.notify60[i].uin; i++)
				free(e->event.notify60[i].descr);

			free(e->event.notify60);

			break;
		}

		case GG_EVENT_STATUS60:
			free(e->event.status60.descr);
			break;

		case GG_EVENT_STATUS:
			free(e->event.status.descr);
			break;

		case GG_EVENT_NOTIFY_DESCR:
			free(e->event.notify_descr.notify);
			free(e->event.notify_descr.descr);
			break;

		case GG_EVENT_DCC_VOICE_DATA:
			free(e->event.dcc_voice_data.data);
			break;

		case GG_EVENT_PUBDIR50_SEARCH_REPLY:
		case GG_EVENT_PUBDIR50_READ:
		case GG_EVENT_PUBDIR50_WRITE:
			gg_pubdir50_free(e->event.pubdir50);
			break;

		case GG_EVENT_USERLIST:
			free(e->event.userlist.reply);
			break;

		case GG_EVENT_IMAGE_REPLY:
			free(e->event.image_reply.filename);
			free(e->event.image_reply.image);
			break;
	}

	free(e);
}

/** \cond internal */

/**
 * \internal Usuwa obrazek z kolejki do wysłania.
 *
 * \param s Struktura sesji
 * \param q Struktura obrazka
 * \param freeq Flaga zwolnienia elementu kolejki
 *
 * \return 0 jeśli się powiodło, -1 jeśli wystąpił błąd
 */
int gg_image_queue_remove(struct gg_session *s, struct gg_image_queue *q, int freeq)
{
	if (!s || !q) {
		errno = EFAULT;
		return -1;
	}

	if (s->images == q)
		s->images = q->next;
	else {
		struct gg_image_queue *qq;

		for (qq = s->images; qq; qq = qq->next) {
			if (qq->next == q) {
				qq->next = q->next;
				break;
			}
		}
	}

	if (freeq) {
		free(q->image);
		free(q->filename);
		free(q);
	}

	return 0;
}

/**
 * \internal Analizuje przychodzący pakiet z obrazkiem.
 *
 * \param e Struktura zdarzenia
 * \param p Bufor z danymi
 * \param len Długość bufora
 * \param sess Struktura sesji
 * \param sender Numer nadawcy
 */
static void gg_image_queue_parse(struct gg_event *e, char *p, unsigned int len, struct gg_session *sess, uin_t sender)
{
	struct gg_msg_image_reply *i = (void*) p;
	struct gg_image_queue *q, *qq;

	if (!p || !sess || !e) {
		errno = EFAULT;
		return;
	}

	/* znajdź dany obrazek w kolejce danej sesji */

	for (qq = sess->images, q = NULL; qq; qq = qq->next) {
		if (sender == qq->sender && i->size == qq->size && i->crc32 == qq->crc32) {
			q = qq;
			break;
		}
	}

	if (!q) {
		gg_debug_session(sess, GG_DEBUG_MISC, "// gg_image_queue_parse() unknown image from %d, size=%d, crc32=%.8x\n", sender, i->size, i->crc32);
		return;
	}

	if (p[0] == 0x05) {
		int i, ok = 0;

		q->done = 0;

		len -= sizeof(struct gg_msg_image_reply);
		p += sizeof(struct gg_msg_image_reply);

		/* sprawdź, czy mamy tekst zakończony \0 */

		for (i = 0; i < (int)len; i++) {
			if (!p[i]) {
				ok = 1;
				break;
			}
		}

		if (!ok) {
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_image_queue_parse() malformed packet from %d, unlimited filename\n", sender);
			return;
		}

		if (!(q->filename = strdup(p))) {
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_image_queue_parse() not enough memory for filename\n");
			return;
		}

		len -= strlen(p) + 1;
		p += strlen(p) + 1;
	} else {
		len -= sizeof(struct gg_msg_image_reply);
		p += sizeof(struct gg_msg_image_reply);
	}

	if (q->done + len > q->size)
		len = q->size - q->done;

	memcpy(q->image + q->done, p, len);
	q->done += len;

	/* jeśli skończono odbierać obrazek, wygeneruj zdarzenie */

	if (q->done >= q->size) {
		e->type = GG_EVENT_IMAGE_REPLY;
		e->event.image_reply.sender = sender;
		e->event.image_reply.size = q->size;
		e->event.image_reply.crc32 = q->crc32;
		e->event.image_reply.filename = q->filename;
		e->event.image_reply.image = q->image;

		gg_image_queue_remove(sess, q, 0);

		free(q);
	}
}

/**
 * \internal Analizuje przychodzący pakiet z wiadomością.
 *
 * Rozbija pakiet na poszczególne składniki -- tekst, informacje
 * o konferencjach, formatowani itd.
 *
 * \param h Wskaźnik do odebranego pakietu
 * \param e Struktura zdarzenia
 * \param sess Struktura sesji
 *
 * \return 0 jeśli się powiodło, -1 w przypadku błędu
 */
static int gg_handle_recv_msg(struct gg_header *h, struct gg_event *e, struct gg_session *sess)
{
	struct gg_recv_msg *r = (struct gg_recv_msg*) ((char*) h + sizeof(struct gg_header));
	char *p, *packet_end = (char*) r + h->length;

	gg_debug_session(sess, GG_DEBUG_FUNCTION, "** gg_handle_recv_msg(%p, %p);\n", h, e);

	if (!r->seq && !r->msgclass) {
		gg_debug_session(sess, GG_DEBUG_MISC, "// gg_handle_recv_msg() oops, silently ignoring the bait\n");
		e->type = GG_EVENT_NONE;
		return 0;
	}

	for (p = (char*) r + sizeof(*r); *p; p++) {
		if (*p == 0x02 && p == packet_end - 1) {
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_handle_recv_msg() received ctcp packet\n");
			break;
		}
		if (p >= packet_end) {
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_handle_recv_msg() malformed packet, message out of bounds (0)\n");
			goto malformed;
		}
	}

	p++;

	/* przeanalizuj dodatkowe opcje */
	while (p < packet_end) {
		switch (*p) {
			case 0x01:		/* konferencja */
			{
				struct gg_msg_recipients *m = (void*) p;
				uint32_t i, count;

				p += sizeof(*m);

				if (p > packet_end) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_handle_recv_msg() packet out of bounds (1)\n");
					goto malformed;
				}

				count = gg_fix32(m->count);

				if (p + count * sizeof(uin_t) > packet_end || p + count * sizeof(uin_t) < p || count > 0xffff) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_handle_recv_msg() packet out of bounds (1.5)\n");
					goto malformed;
				}

				if (!(e->event.msg.recipients = (void*) malloc(count * sizeof(uin_t)))) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_handle_recv_msg() not enough memory for recipients data\n");
					goto fail;
				}

				for (i = 0; i < count; i++, p += sizeof(uint32_t)) {
					uint32_t u;
					memcpy(&u, p, sizeof(uint32_t));
					e->event.msg.recipients[i] = gg_fix32(u);
				}

				e->event.msg.recipients_count = count;

				break;
			}

			case 0x02:		/* richtext */
			{
				uint16_t len;
				char *buf;

				if (p + 3 > packet_end) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_handle_recv_msg() packet out of bounds (2)\n");
					goto malformed;
				}

				memcpy(&len, p + 1, sizeof(uint16_t));
				len = gg_fix16(len);

				if (!(buf = malloc(len))) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_handle_recv_msg() not enough memory for richtext data\n");
					goto fail;
				}

				p += 3;

				if (p + len > packet_end) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_handle_recv_msg() packet out of bounds (3)\n");
					free(buf);
					goto malformed;
				}

				memcpy(buf, p, len);

				e->event.msg.formats = buf;
				e->event.msg.formats_length = len;

				p += len;

				break;
			}

			case 0x04:		/* image_request */
			{
				struct gg_msg_image_request *i = (void*) p;

				if (p + sizeof(*i) > packet_end) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_handle_recv_msg() packet out of bounds (3)\n");
					goto malformed;
				}

				e->event.image_request.sender = gg_fix32(r->sender);
				e->event.image_request.size = gg_fix32(i->size);
				e->event.image_request.crc32 = gg_fix32(i->crc32);

				e->type = GG_EVENT_IMAGE_REQUEST;

				return 0;
			}

			case 0x05:		/* image_reply */
			case 0x06:
			{
				struct gg_msg_image_reply *rep = (void*) p;

				if (p + sizeof(struct gg_msg_image_reply) == packet_end) {

					/* pusta odpowiedź - klient po drugiej stronie nie ma żądanego obrazka */

					e->type = GG_EVENT_IMAGE_REPLY;
					e->event.image_reply.sender = gg_fix32(r->sender);
					e->event.image_reply.size = 0;
					e->event.image_reply.crc32 = gg_fix32(rep->crc32);
					e->event.image_reply.filename = NULL;
					e->event.image_reply.image = NULL;
					return 0;

				} else if (p + sizeof(struct gg_msg_image_reply) + 1 > packet_end) {

					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_handle_recv_msg() packet out of bounds (4)\n");
					goto malformed;
				}

				rep->size = gg_fix32(rep->size);
				rep->crc32 = gg_fix32(rep->crc32);
				gg_image_queue_parse(e, p, (unsigned int)(packet_end - p), sess, gg_fix32(r->sender));

				return 0;
			}

			default:
			{
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_handle_recv_msg() unknown payload 0x%.2x\n", *p);
				p = packet_end;
			}
		}
	}

	e->type = GG_EVENT_MSG;
	e->event.msg.msgclass = gg_fix32(r->msgclass);
	e->event.msg.sender = gg_fix32(r->sender);
	e->event.msg.time = gg_fix32(r->time);
	e->event.msg.message = (unsigned char*) strdup((char*) r + sizeof(*r));

	return 0;

malformed:
	e->type = GG_EVENT_NONE;

	free(e->event.msg.recipients);
	free(e->event.msg.formats);

	return 0;

fail:
	free(e->event.msg.recipients);
	free(e->event.msg.formats);
	return -1;
}

/**
 * \internal Odbiera pakiet od serwera.
 *
 * Analizuje pakiet i wypełnia strukturę zdarzenia.
 *
 * \param sess Struktura sesji
 * \param e Struktura zdarzenia
 *
 * \return 0 jeśli się powiodło, -1 jeśli wystąpił błąd
 */
static int gg_watch_fd_connected(struct gg_session *sess, struct gg_event *e)
{
	struct gg_header *h = NULL;
	char *p;

	gg_debug_session(sess, GG_DEBUG_FUNCTION, "** gg_watch_fd_connected(%p, %p);\n", sess, e);

	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	if (!(h = gg_recv_packet(sess))) {
		gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() gg_recv_packet failed (errno=%d, %s)\n", errno, strerror(errno));
		goto fail;
	}

	p = (char*) h + sizeof(struct gg_header);

	switch (h->type) {
		case GG_RECV_MSG:
		{
			if (h->length >= sizeof(struct gg_recv_msg))
				if (gg_handle_recv_msg(h, e, sess))
					goto fail;

			break;
		}

		case GG_NOTIFY_REPLY:
		{
			struct gg_notify_reply *n = (void*) p;
			unsigned int count, i;
			char *tmp;

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received a notify reply\n");

			if (h->length < sizeof(*n)) {
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() incomplete packet\n");
				errno = EINVAL;
				goto fail;
			}

			if (gg_fix32(n->status) == GG_STATUS_BUSY_DESCR || gg_fix32(n->status) == GG_STATUS_NOT_AVAIL_DESCR || gg_fix32(n->status) == GG_STATUS_AVAIL_DESCR) {
				e->type = GG_EVENT_NOTIFY_DESCR;

				if (!(e->event.notify_descr.notify = (void*) malloc(sizeof(*n) * 2))) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() not enough memory for notify data\n");
					goto fail;
				}
				e->event.notify_descr.notify[1].uin = 0;
				memcpy(e->event.notify_descr.notify, p, sizeof(*n));
				e->event.notify_descr.notify[0].uin = gg_fix32(e->event.notify_descr.notify[0].uin);
				e->event.notify_descr.notify[0].status = gg_fix32(e->event.notify_descr.notify[0].status);
				e->event.notify_descr.notify[0].remote_port = gg_fix16(e->event.notify_descr.notify[0].remote_port);

				count = h->length - sizeof(*n);
				if (!(tmp = malloc(count + 1))) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() not enough memory for notify data\n");
					goto fail;
				}
				memcpy(tmp, p + sizeof(*n), count);
				tmp[count] = 0;
				e->event.notify_descr.descr = tmp;

			} else {
				e->type = GG_EVENT_NOTIFY;

				if (!(e->event.notify = (void*) malloc(h->length + 2 * sizeof(*n)))) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() not enough memory for notify data\n");
					goto fail;
				}

				memcpy(e->event.notify, p, h->length);
				count = h->length / sizeof(*n);
				e->event.notify[count].uin = 0;

				for (i = 0; i < count; i++) {
					e->event.notify[i].uin = gg_fix32(e->event.notify[i].uin);
					e->event.notify[i].status = gg_fix32(e->event.notify[i].status);
					e->event.notify[i].remote_port = gg_fix16(e->event.notify[i].remote_port);
				}
			}

			break;
		}

		case GG_STATUS:
		{
			struct gg_status *s = (void*) p;

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received a status change\n");

			if (h->length >= sizeof(*s)) {
				e->type = GG_EVENT_STATUS;
				memcpy(&e->event.status, p, sizeof(*s));
				e->event.status.uin = gg_fix32(e->event.status.uin);
				e->event.status.status = gg_fix32(e->event.status.status);
				if (h->length > sizeof(*s)) {
					int len = h->length - sizeof(*s);
					char *buf = malloc(len + 1);
					if (buf) {
						memcpy(buf, p + sizeof(*s), len);
						buf[len] = 0;
					}
					e->event.status.descr = buf;
				} else
					e->event.status.descr = NULL;
			}

			break;
		}

		case GG_NOTIFY_REPLY77:
		{
			struct gg_notify_reply77 *n = (void*) p;
			unsigned int length = h->length, i = 0;

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received a notify reply\n");

			e->type = GG_EVENT_NOTIFY60;
			e->event.notify60 = malloc(sizeof(*e->event.notify60));

			if (!e->event.notify60) {
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() not enough memory for notify data\n");
				goto fail;
			}

			e->event.notify60[0].uin = 0;

			while (length >= sizeof(struct gg_notify_reply77)) {
				uin_t uin = gg_fix32(n->uin);
				char *tmp;

				e->event.notify60[i].uin = uin & 0x00ffffff;
				e->event.notify60[i].status = n->status;
				e->event.notify60[i].remote_ip = n->remote_ip;
				e->event.notify60[i].remote_port = gg_fix16(n->remote_port);
				e->event.notify60[i].version = n->version;
				e->event.notify60[i].image_size = n->image_size;
				e->event.notify60[i].descr = NULL;
				e->event.notify60[i].time = 0;

				if (uin & 0x40000000)
					e->event.notify60[i].version |= GG_HAS_AUDIO_MASK;
				if (uin & 0x20000000)
					e->event.notify60[i].version |= GG_HAS_AUDIO7_MASK;
				if (uin & 0x08000000)
					e->event.notify60[i].version |= GG_ERA_OMNIX_MASK;

				if (GG_S_D(n->status)) {
					unsigned char descr_len = *((char*) n + sizeof(struct gg_notify_reply77));

					if (descr_len < length) {
						if (!(e->event.notify60[i].descr = malloc(descr_len + 1))) {
							gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() not enough memory for notify data\n");
							goto fail;
						}

						memcpy(e->event.notify60[i].descr, (char*) n + sizeof(struct gg_notify_reply77) + 1, descr_len);
						e->event.notify60[i].descr[descr_len] = 0;

						/* XXX czas */
							
						length -= sizeof(struct gg_notify_reply77) + descr_len + 1;
						n = (void*) ((char*) n + sizeof(struct gg_notify_reply77) + descr_len + 1);
					} else {
						length = 0;
					}

				} else {
					length -= sizeof(struct gg_notify_reply77);
					n = (void*) ((char*) n + sizeof(struct gg_notify_reply77));
				}

				if (!(tmp = realloc(e->event.notify60, (i + 2) * sizeof(*e->event.notify60)))) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() not enough memory for notify data\n");
					free(e->event.notify60);
					goto fail;
				}

				e->event.notify60 = (void*) tmp;
				e->event.notify60[++i].uin = 0;
			}

			break;
		}

		case GG_STATUS77:
		{
			struct gg_status77 *s = (void*) p;
			uint32_t uin;

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received a status change\n");

			if (h->length < sizeof(*s))
				break;

			uin = gg_fix32(s->uin);

			e->type = GG_EVENT_STATUS60;
			e->event.status60.uin = uin & 0x00ffffff;
			e->event.status60.status = s->status;
			e->event.status60.remote_ip = s->remote_ip;
			e->event.status60.remote_port = gg_fix16(s->remote_port);
			e->event.status60.version = s->version;
			e->event.status60.image_size = s->image_size;
			e->event.status60.descr = NULL;
			e->event.status60.time = 0;

			if (uin & 0x40000000)
				e->event.status60.version |= GG_HAS_AUDIO_MASK;
			if (uin & 0x20000000)
				e->event.status60.version |= GG_HAS_AUDIO7_MASK;
			if (uin & 0x08000000)
				e->event.status60.version |= GG_ERA_OMNIX_MASK;

			if (h->length > sizeof(*s)) {
				int len = h->length - sizeof(*s);
				char *buf = malloc(len + 1);

				if (buf) {
					memcpy(buf, (char*) p + sizeof(*s), len);
					buf[len] = 0;
				}

				e->event.status60.descr = buf;

				if (len > 4 && p[h->length - 5] == 0) {
					uint32_t t;
					memcpy(&t, p + h->length - 4, sizeof(uint32_t));
					e->event.status60.time = gg_fix32(t);
				}
			}

			break;
		}

		case GG_NOTIFY_REPLY60:
		{
			struct gg_notify_reply60 *n = (void*) p;
			unsigned int length = h->length, i = 0;

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received a notify reply\n");

			e->type = GG_EVENT_NOTIFY60;
			e->event.notify60 = malloc(sizeof(*e->event.notify60));

			if (!e->event.notify60) {
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() not enough memory for notify data\n");
				goto fail;
			}

			e->event.notify60[0].uin = 0;

			while (length >= sizeof(struct gg_notify_reply60)) {
				uin_t uin = gg_fix32(n->uin);
				char *tmp;

				e->event.notify60[i].uin = uin & 0x00ffffff;
				e->event.notify60[i].status = n->status;
				e->event.notify60[i].remote_ip = n->remote_ip;
				e->event.notify60[i].remote_port = gg_fix16(n->remote_port);
				e->event.notify60[i].version = n->version;
				e->event.notify60[i].image_size = n->image_size;
				e->event.notify60[i].descr = NULL;
				e->event.notify60[i].time = 0;

				if (uin & 0x40000000)
					e->event.notify60[i].version |= GG_HAS_AUDIO_MASK;
				if (uin & 0x08000000)
					e->event.notify60[i].version |= GG_ERA_OMNIX_MASK;

				if (GG_S_D(n->status)) {
					unsigned char descr_len = *((char*) n + sizeof(struct gg_notify_reply60));

					if (descr_len < length) {
						if (!(e->event.notify60[i].descr = malloc(descr_len + 1))) {
							gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() not enough memory for notify data\n");
							goto fail;
						}

						memcpy(e->event.notify60[i].descr, (char*) n + sizeof(struct gg_notify_reply60) + 1, descr_len);
						e->event.notify60[i].descr[descr_len] = 0;

						/* XXX czas */
							
						length -= sizeof(struct gg_notify_reply60) + descr_len + 1;
						n = (void*) ((char*) n + sizeof(struct gg_notify_reply60) + descr_len + 1);
					} else {
						length = 0;
					}

				} else {
					length -= sizeof(struct gg_notify_reply60);
					n = (void*) ((char*) n + sizeof(struct gg_notify_reply60));
				}

				if (!(tmp = realloc(e->event.notify60, (i + 2) * sizeof(*e->event.notify60)))) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() not enough memory for notify data\n");
					free(e->event.notify60);
					goto fail;
				}

				e->event.notify60 = (void*) tmp;
				e->event.notify60[++i].uin = 0;
			}

			break;
		}

		case GG_STATUS60:
		{
			struct gg_status60 *s = (void*) p;
			uint32_t uin;

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received a status change\n");

			if (h->length < sizeof(*s))
				break;

			uin = gg_fix32(s->uin);

			e->type = GG_EVENT_STATUS60;
			e->event.status60.uin = uin & 0x00ffffff;
			e->event.status60.status = s->status;
			e->event.status60.remote_ip = s->remote_ip;
			e->event.status60.remote_port = gg_fix16(s->remote_port);
			e->event.status60.version = s->version;
			e->event.status60.image_size = s->image_size;
			e->event.status60.descr = NULL;
			e->event.status60.time = 0;

			if (uin & 0x40000000)
				e->event.status60.version |= GG_HAS_AUDIO_MASK;
			if (uin & 0x08000000)
				e->event.status60.version |= GG_ERA_OMNIX_MASK;

			if (h->length > sizeof(*s)) {
				int len = h->length - sizeof(*s);
				char *buf = malloc(len + 1);

				if (buf) {
					memcpy(buf, (char*) p + sizeof(*s), len);
					buf[len] = 0;
				}

				e->event.status60.descr = buf;

				if (len > 4 && p[h->length - 5] == 0) {
					uint32_t t;
					memcpy(&t, p + h->length - 4, sizeof(uint32_t));
					e->event.status60.time = gg_fix32(t);
				}
			}

			break;
		}

		case GG_SEND_MSG_ACK:
		{
			struct gg_send_msg_ack *s = (void*) p;

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received a message ack\n");

			if (h->length < sizeof(*s))
				break;

			e->type = GG_EVENT_ACK;
			e->event.ack.status = gg_fix32(s->status);
			e->event.ack.recipient = gg_fix32(s->recipient);
			e->event.ack.seq = gg_fix32(s->seq);

			break;
		}

		case GG_PONG:
		{
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received a pong\n");

			e->type = GG_EVENT_PONG;
			sess->last_pong = time(NULL);

			break;
		}

		case GG_DISCONNECTING:
		{
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received disconnection warning\n");
			e->type = GG_EVENT_DISCONNECT;
			break;
		}

		case GG_PUBDIR50_REPLY:
		{
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received pubdir/search reply\n");
			if (gg_pubdir50_handle_reply(e, p, h->length) == -1)
				goto fail;
			break;
		}

		case GG_USERLIST_REPLY:
		{
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received userlist reply\n");

			if (h->length < 1)
				break;

			/* jeśli odpowiedź na eksport, wywołaj zdarzenie tylko
			 * gdy otrzymano wszystkie odpowiedzi */
			if (p[0] == GG_USERLIST_PUT_REPLY || p[0] == GG_USERLIST_PUT_MORE_REPLY) {
				if (--sess->userlist_blocks)
					break;

				p[0] = GG_USERLIST_PUT_REPLY;
			}

			if (h->length > 1) {
				char *tmp;
				unsigned int len = (sess->userlist_reply) ? strlen(sess->userlist_reply) : 0;

				gg_debug_session(sess, GG_DEBUG_MISC, "userlist_reply=%p, len=%d\n", sess->userlist_reply, len);

				if (!(tmp = realloc(sess->userlist_reply, len + h->length))) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() not enough memory for userlist reply\n");
					free(sess->userlist_reply);
					sess->userlist_reply = NULL;
					goto fail;
				}

				sess->userlist_reply = tmp;
				sess->userlist_reply[len + h->length - 1] = 0;
				memcpy(sess->userlist_reply + len, p + 1, h->length - 1);
			}

			if (p[0] == GG_USERLIST_GET_MORE_REPLY)
				break;

			e->type = GG_EVENT_USERLIST;
			e->event.userlist.type = p[0];
			e->event.userlist.reply = sess->userlist_reply;
			sess->userlist_reply = NULL;

			break;
		}

		case GG_DCC7_ID_REPLY:
		{
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received dcc7 id packet\n");

			if (h->length < sizeof(struct gg_dcc7_id_reply))
				break;

			if (gg_dcc7_handle_id(sess, e, p, h->length) == -1)
				goto fail;

			break;
		}

		case GG_DCC7_ACCEPT:
		{
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received dcc7 accept\n");

			if (h->length < sizeof(struct gg_dcc7_accept))
				break;

			if (gg_dcc7_handle_accept(sess, e, p, h->length) == -1)
				goto fail;

			break;
		}

		case GG_DCC7_NEW:
		{
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received dcc7 request\n");

			if (h->length < sizeof(struct gg_dcc7_new))
				break;

			if (gg_dcc7_handle_new(sess, e, p, h->length) == -1)
				goto fail;

			break;
		}

		case GG_DCC7_REJECT:
		{
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received dcc7 reject\n");

			if (h->length < sizeof(struct gg_dcc7_reject))
				break;

			if (gg_dcc7_handle_reject(sess, e, p, h->length) == -1)
				goto fail;

			break;
		}

		case GG_DCC7_INFO:
		{
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received dcc7 info\n");

			if (h->length < sizeof(struct gg_dcc7_info))
				break;

			if (gg_dcc7_handle_info(sess, e, p, h->length) == -1)
				goto fail;

			break;
		}

		default:
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd_connected() received unknown packet 0x%.2x\n", h->type);
	}

	free(h);
	return 0;

fail:
	free(h);
	return -1;
}

/** \endcond */

/**
 * Funkcja wywoływana po zaobserwowaniu zmian na deskryptorze sesji.
 *
 * Funkcja zwraca strukturę zdarzenia \c gg_event. Jeśli rodzaj zdarzenia
 * to \c GG_EVENT_NONE, nie wydarzyło się jeszcze nic wartego odnotowania.
 * Strukturę zdarzenia należy zwolnić funkcja \c gg_event_free.
 *
 * \param sess Struktura sesji
 *
 * \return Struktura zdarzenia lub \c NULL jeśli wystąpił błąd
 */
struct gg_event *gg_watch_fd(struct gg_session *sess)
{
	struct gg_event *e;
	int res = 0;
	int port = 0;
	int errno2 = 0;

	gg_debug_session(sess, GG_DEBUG_FUNCTION, "** gg_watch_fd(%p);\n", sess);

	if (!sess) {
		errno = EFAULT;
		return NULL;
	}

	if (!(e = (void*) calloc(1, sizeof(*e)))) {
		gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() not enough memory for event data\n");
		return NULL;
	}

	e->type = GG_EVENT_NONE;

	if (sess->send_buf && (sess->state == GG_STATE_READING_REPLY || sess->state == GG_STATE_CONNECTED)) {
		gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() sending %d bytes of queued data\n", sess->send_left);

		res = write(sess->fd, sess->send_buf, sess->send_left);

		if (res == -1 && errno != EAGAIN) {
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() write() failed (errno=%d, %s)\n", errno, strerror(errno));

			if (sess->state == GG_STATE_READING_REPLY)
				goto fail_connecting;
			else
				goto done;
		}

		if (res == sess->send_left) {
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() sent all queued data\n");
			free(sess->send_buf);
			sess->send_buf = NULL;
			sess->send_left = 0;
		} else if (res > 0) {
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() sent %d bytes of queued data, %d bytes left\n", res, sess->send_left - res);

			memmove(sess->send_buf, sess->send_buf + res, sess->send_left - res);
			sess->send_left -= res;
		}
	}

	switch (sess->state) {
		case GG_STATE_RESOLVING:
		{
			struct in_addr addr;
			int failed = 0;

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() GG_STATE_RESOLVING\n");

			if (read(sess->fd, &addr, sizeof(addr)) < (signed)sizeof(addr) || addr.s_addr == INADDR_NONE) {
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() resolving failed\n");
				failed = 1;
				errno2 = errno;
			}

			close(sess->fd);
			sess->fd = -1;

#ifndef GG_CONFIG_HAVE_PTHREAD
			waitpid(sess->pid, NULL, 0);
			sess->pid = -1;
#else
			if (sess->resolver) {
				gg_resolve_pthread_cleanup(sess->resolver, 0);
				sess->resolver = NULL;
			}
#endif

			if (failed) {
				errno = errno2;
				goto fail_resolving;
			}

			/* jeśli jesteśmy w resolverze i mamy ustawiony port
			 * proxy, znaczy, że resolvowaliśmy proxy. zatem
			 * wpiszmy jego adres. */
			if (sess->proxy_port)
				sess->proxy_addr = addr.s_addr;

			/* zapiszmy sobie adres huba i adres serwera (do
			 * bezpośredniego połączenia, jeśli hub leży)
			 * z resolvera. */
			if (sess->proxy_addr && sess->proxy_port)
				port = sess->proxy_port;
			else {
				sess->server_addr = sess->hub_addr = addr.s_addr;
				port = GG_APPMSG_PORT;
			}

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() resolved, connecting to %s:%d\n", inet_ntoa(addr), port);

			/* łączymy się albo z hubem, albo z proxy, zależnie
			 * od tego, co resolvowaliśmy. */
			if ((sess->fd = gg_connect(&addr, port, sess->async)) == -1) {
				/* jeśli w trybie asynchronicznym gg_connect()
				 * zwróci błąd, nie ma sensu próbować dalej. */
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connection failed (errno=%d, %s), critical\n", errno, strerror(errno));
				goto fail_connecting;
			}

			/* jeśli podano serwer i łączmy się przez proxy,
			 * jest to bezpośrednie połączenie, inaczej jest
			 * do huba. */

			if (sess->proxy_addr && sess->proxy_port && sess->server_addr) {
				sess->state = GG_STATE_CONNECTING_GG;
				sess->soft_timeout = 1;
			} else
				sess->state = GG_STATE_CONNECTING_HUB;

			sess->check = GG_CHECK_WRITE;
			sess->timeout = GG_DEFAULT_TIMEOUT;

			break;
		}

		case GG_STATE_CONNECTING_HUB:
		{
			char buf[1024], *client, *auth;
			int res = 0;
			unsigned int res_size = sizeof(res);
			const char *host, *appmsg;

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() GG_STATE_CONNECTING_HUB\n");

			/* jeśli asynchroniczne, sprawdzamy, czy nie wystąpił
			 * przypadkiem jakiś błąd. */
			if (sess->async && (getsockopt(sess->fd, SOL_SOCKET, SO_ERROR, (char *)&res, &res_size) || res)) {
				/* no tak, nie udało się połączyć z proxy. nawet
				 * nie próbujemy dalej. */
				if (sess->proxy_addr && sess->proxy_port) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connection to proxy failed (errno=%d, %s)\n", res, strerror(res));
					goto fail_connecting;
				}

				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connection to hub failed (errno=%d, %s), trying direct connection\n", res, strerror(res));
				close(sess->fd);

				if ((sess->fd = gg_connect(&sess->hub_addr, GG_DEFAULT_PORT, sess->async)) == -1) {
					/* przy asynchronicznych, gg_connect()
					 * zwraca -1 przy błędach socket(),
					 * ioctl(), braku routingu itd. dlatego
					 * nawet nie próbujemy dalej. */
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() direct connection failed (errno=%d, %s), critical\n", errno, strerror(errno));
					goto fail_connecting;
				}

				sess->state = GG_STATE_CONNECTING_GG;
				sess->check = GG_CHECK_WRITE;
				sess->timeout = GG_DEFAULT_TIMEOUT;
				sess->soft_timeout = 1;
				break;
			}

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connected to hub, sending query\n");

			if (!(client = gg_urlencode((sess->client_version) ? sess->client_version : GG_DEFAULT_CLIENT_VERSION))) {
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() out of memory for client version\n");
				goto fail_connecting;
			}

			if (!gg_proxy_http_only && sess->proxy_addr && sess->proxy_port)
				host = "http://" GG_APPMSG_HOST;
			else
				host = "";

#ifdef GG_CONFIG_HAVE_OPENSSL
			if (sess->ssl)
				appmsg = "appmsg3.asp";
			else
#endif
				appmsg = "appmsg2.asp";

			auth = gg_proxy_auth();

			snprintf(buf, sizeof(buf) - 1,
				"GET %s/appsvc/%s?fmnumber=%u&version=%s&lastmsg=%d HTTP/1.0\r\n"
				"Host: " GG_APPMSG_HOST "\r\n"
				"User-Agent: " GG_HTTP_USERAGENT "\r\n"
				"Pragma: no-cache\r\n"
				"%s"
				"\r\n", host, appmsg, sess->uin, client, sess->last_sysmsg, (auth) ? auth : "");

			if (auth)
				free(auth);

			free(client);

			/* zwolnij pamięć po wersji klienta. */
			if (sess->client_version) {
				free(sess->client_version);
				sess->client_version = NULL;
			}

			gg_debug_session(sess, GG_DEBUG_MISC, "=> -----BEGIN-HTTP-QUERY-----\n%s\n=> -----END-HTTP-QUERY-----\n", buf);

			/* zapytanie jest krótkie, więc zawsze zmieści się
			 * do bufora gniazda. jeśli write() zwróci mniej,
			 * stało się coś złego. */
			if (write(sess->fd, buf, strlen(buf)) < (signed)strlen(buf)) {
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() sending query failed\n");

				e->type = GG_EVENT_CONN_FAILED;
				e->event.failure = GG_FAILURE_WRITING;
				sess->state = GG_STATE_IDLE;
				close(sess->fd);
				sess->fd = -1;
				break;
			}

			sess->state = GG_STATE_READING_DATA;
			sess->check = GG_CHECK_READ;
			sess->timeout = GG_DEFAULT_TIMEOUT;

			break;
		}

		case GG_STATE_READING_DATA:
		{
			char buf[1024], *tmp, *host;
			int port = GG_DEFAULT_PORT;
			struct in_addr addr;

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() GG_STATE_READING_DATA\n");

			/* czytamy linię z gniazda i obcinamy \r\n. */
			gg_read_line(sess->fd, buf, sizeof(buf) - 1);
			gg_chomp(buf);
			gg_debug_session(sess, GG_DEBUG_TRAFFIC, "// gg_watch_fd() received http header (%s)\n", buf);

			/* sprawdzamy, czy wszystko w porządku. */
			if (strncmp(buf, "HTTP/1.", 7) || strncmp(buf + 9, "200", 3)) {
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() that's not what we've expected, trying direct connection\n");

				close(sess->fd);

				/* jeśli otrzymaliśmy jakieś dziwne informacje,
				 * próbujemy się łączyć z pominięciem huba. */
				if (sess->proxy_addr && sess->proxy_port) {
					if ((sess->fd = gg_connect(&sess->proxy_addr, sess->proxy_port, sess->async)) == -1) {
						/* trudno. nie wyszło. */
						gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connection to proxy failed (errno=%d, %s)\n", errno, strerror(errno));
						goto fail_connecting;
					}

					sess->state = GG_STATE_CONNECTING_GG;
					sess->check = GG_CHECK_WRITE;
					sess->timeout = GG_DEFAULT_TIMEOUT;
					sess->soft_timeout = 1;
					break;
				}

				sess->port = GG_DEFAULT_PORT;

				/* łączymy się na port 8074 huba. */
				if ((sess->fd = gg_connect(&sess->hub_addr, sess->port, sess->async)) == -1) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connection failed (errno=%d, %s), trying https\n", errno, strerror(errno));

					sess->port = GG_HTTPS_PORT;

					/* łączymy się na port 443. */
					if ((sess->fd = gg_connect(&sess->hub_addr, sess->port, sess->async)) == -1) {
						gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connection failed (errno=%d, %s)\n", errno, strerror(errno));
						goto fail_connecting;
					}
				}

				sess->state = GG_STATE_CONNECTING_GG;
				sess->check = GG_CHECK_WRITE;
				sess->timeout = GG_DEFAULT_TIMEOUT;
				sess->soft_timeout = 1;
				break;
			}

			/* ignorujemy resztę nagłówka. */
			while (strcmp(buf, "\r\n") && strcmp(buf, ""))
				gg_read_line(sess->fd, buf, sizeof(buf) - 1);

			/* czytamy pierwszą linię danych. */
			gg_read_line(sess->fd, buf, sizeof(buf) - 1);
			gg_chomp(buf);

			/* jeśli pierwsza liczba w linii nie jest równa zeru,
			 * oznacza to, że mamy wiadomość systemową. */
			if (atoi(buf)) {
				char tmp[1024], *foo, *sysmsg_buf = NULL;
				int len = 0;

				while (gg_read_line(sess->fd, tmp, sizeof(tmp) - 1)) {
					if (!(foo = realloc(sysmsg_buf, len + strlen(tmp) + 2))) {
						gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() out of memory for system message, ignoring\n");
						break;
					}

					sysmsg_buf = foo;

					if (!len)
						strcpy(sysmsg_buf, tmp);
					else
						strcat(sysmsg_buf, tmp);

					len += strlen(tmp);
				}

				e->type = GG_EVENT_MSG;
				e->event.msg.msgclass = atoi(buf);
				e->event.msg.sender = 0;
				e->event.msg.message = (unsigned char*) sysmsg_buf;
			}

			close(sess->fd);

			gg_debug_session(sess, GG_DEBUG_TRAFFIC, "// gg_watch_fd() received http data (%s)\n", buf);

			/* analizujemy otrzymane dane. */
			tmp = buf;

			while (*tmp && *tmp != ' ')
				tmp++;
			while (*tmp && *tmp == ' ')
				tmp++;
			host = tmp;
			while (*tmp && *tmp != ' ')
				tmp++;
			*tmp = 0;

			if ((tmp = strchr(host, ':'))) {
				*tmp = 0;
				port = atoi(tmp + 1);
			}

			if (!strcmp(host, "notoperating")) {
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() service unavailable\n", errno, strerror(errno));
				sess->fd = -1;
				goto fail_unavailable;
			}

			addr.s_addr = inet_addr(host);
			sess->server_addr = addr.s_addr;

			if (!gg_proxy_http_only && sess->proxy_addr && sess->proxy_port) {
				/* jeśli mamy proxy, łączymy się z nim. */
				if ((sess->fd = gg_connect(&sess->proxy_addr, sess->proxy_port, sess->async)) == -1) {
					/* nie wyszło? trudno. */
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connection to proxy failed (errno=%d, %s)\n", errno, strerror(errno));
					goto fail_connecting;
				}

				sess->state = GG_STATE_CONNECTING_GG;
				sess->check = GG_CHECK_WRITE;
				sess->timeout = GG_DEFAULT_TIMEOUT;
				sess->soft_timeout = 1;
				break;
			}

			sess->port = port;

			/* łączymy się z właściwym serwerem. */
			if ((sess->fd = gg_connect(&addr, sess->port, sess->async)) == -1) {
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connection failed (errno=%d, %s), trying https\n", errno, strerror(errno));

				sess->port = GG_HTTPS_PORT;

				/* nie wyszło? próbujemy portu 443. */
				if ((sess->fd = gg_connect(&addr, GG_HTTPS_PORT, sess->async)) == -1) {
					/* ostatnia deska ratunku zawiodła?
					 * w takim razie zwijamy manatki. */
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connection failed (errno=%d, %s)\n", errno, strerror(errno));
					goto fail_connecting;
				}
			}

			sess->state = GG_STATE_CONNECTING_GG;
			sess->check = GG_CHECK_WRITE;
			sess->timeout = GG_DEFAULT_TIMEOUT;
			sess->soft_timeout = 1;

			break;
		}

		case GG_STATE_CONNECTING_GG:
		{
			int res = 0;
			unsigned int res_size = sizeof(res);

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() GG_STATE_CONNECTING_GG\n");

			sess->soft_timeout = 0;

			/* jeśli wystąpił błąd podczas łączenia się... */
			if (sess->async && (sess->timeout == 0 || getsockopt(sess->fd, SOL_SOCKET, SO_ERROR, (char *)&res, &res_size) || res)) {
				/* jeśli nie udało się połączenie z proxy,
				 * nie mamy czego próbować więcej. */
				if (sess->proxy_addr && sess->proxy_port) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connection to proxy failed (errno=%d, %s)\n", res, strerror(res));
					goto fail_connecting;
				}

				close(sess->fd);
				sess->fd = -1;

#ifdef ETIMEDOUT
				if (sess->timeout == 0)
					errno = ETIMEDOUT;
#endif

#ifdef GG_CONFIG_HAVE_OPENSSL
				/* jeśli logujemy się po TLS, nie próbujemy
				 * się łączyć już z niczym innym w przypadku
				 * błędu. nie dość, że nie ma sensu, to i
				 * trzeba by się bawić w tworzenie na nowo
				 * SSL i SSL_CTX. */

				if (sess->ssl) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connection failed (errno=%d, %s)\n", res, strerror(res));
					goto fail_connecting;
				}
#endif

				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connection failed (errno=%d, %s), trying https\n", res, strerror(res));

				sess->port = GG_HTTPS_PORT;

				/* próbujemy na port 443. */
				if ((sess->fd = gg_connect(&sess->server_addr, sess->port, sess->async)) == -1) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connection failed (errno=%d, %s)\n", errno, strerror(errno));
					goto fail_connecting;
				}
			}

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() connected\n");

			if (gg_proxy_http_only)
				sess->proxy_port = 0;

			/* jeśli mamy proxy, wyślijmy zapytanie. */
			if (sess->proxy_addr && sess->proxy_port) {
				char buf[100], *auth = gg_proxy_auth();
				struct in_addr addr;

				if (sess->server_addr)
					addr.s_addr = sess->server_addr;
				else
					addr.s_addr = sess->hub_addr;

				snprintf(buf, sizeof(buf), "CONNECT %s:%d HTTP/1.0\r\n", inet_ntoa(addr), sess->port);

				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() proxy request:\n//   %s", buf);

				/* wysyłamy zapytanie. jest ono na tyle krótkie,
				 * że musi się zmieścić w buforze gniazda. jeśli
				 * write() zawiedzie, stało się coś złego. */
				if (write(sess->fd, buf, strlen(buf)) < (signed)strlen(buf)) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() can't send proxy request\n");
					if (auth)
						free(auth);
					goto fail_connecting;
				}

				if (auth) {
					gg_debug_session(sess, GG_DEBUG_MISC, "//   %s", auth);
					if (write(sess->fd, auth, strlen(auth)) < (signed)strlen(auth)) {
						gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() can't send proxy request\n");
						free(auth);
						goto fail_connecting;
					}

					free(auth);
				}

				if (write(sess->fd, "\r\n", 2) < 2) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() can't send proxy request\n");
					goto fail_connecting;
				}
			}

#ifdef GG_CONFIG_HAVE_OPENSSL
			if (sess->ssl) {
				SSL_set_fd(sess->ssl, sess->fd);

				sess->state = GG_STATE_TLS_NEGOTIATION;
				sess->check = GG_CHECK_WRITE;
				sess->timeout = GG_DEFAULT_TIMEOUT;

				break;
			}
#endif

			sess->state = GG_STATE_READING_KEY;
			sess->check = GG_CHECK_READ;
			sess->timeout = GG_DEFAULT_TIMEOUT;

			break;
		}

#ifdef GG_CONFIG_HAVE_OPENSSL
		case GG_STATE_TLS_NEGOTIATION:
		{
			int res;
			X509 *peer;

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() GG_STATE_TLS_NEGOTIATION\n");

			if ((res = SSL_connect(sess->ssl)) <= 0) {
				int err = SSL_get_error(sess->ssl, res);

				if (res == 0) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() disconnected during TLS negotiation\n");

					e->type = GG_EVENT_CONN_FAILED;
					e->event.failure = GG_FAILURE_TLS;
					sess->state = GG_STATE_IDLE;
					close(sess->fd);
					sess->fd = -1;
					break;
				}

				if (err == SSL_ERROR_WANT_READ) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() SSL_connect() wants to read\n");

					sess->state = GG_STATE_TLS_NEGOTIATION;
					sess->check = GG_CHECK_READ;
					sess->timeout = GG_DEFAULT_TIMEOUT;

					break;
				} else if (err == SSL_ERROR_WANT_WRITE) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() SSL_connect() wants to write\n");

					sess->state = GG_STATE_TLS_NEGOTIATION;
					sess->check = GG_CHECK_WRITE;
					sess->timeout = GG_DEFAULT_TIMEOUT;

					break;
				} else {
					char buf[1024];

					ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));

					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() SSL_connect() bailed out: %s\n", buf);

					e->type = GG_EVENT_CONN_FAILED;
					e->event.failure = GG_FAILURE_TLS;
					sess->state = GG_STATE_IDLE;
					close(sess->fd);
					sess->fd = -1;
					break;
				}
			}

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() TLS negotiation succeded:\n//   cipher: %s\n", SSL_get_cipher_name(sess->ssl));

			peer = SSL_get_peer_certificate(sess->ssl);

			if (!peer)
				gg_debug_session(sess, GG_DEBUG_MISC, "//   WARNING! unable to get peer certificate!\n");
			else {
				char buf[1024];

				X509_NAME_oneline(X509_get_subject_name(peer), buf, sizeof(buf));
				gg_debug_session(sess, GG_DEBUG_MISC, "//   cert subject: %s\n", buf);

				X509_NAME_oneline(X509_get_issuer_name(peer), buf, sizeof(buf));
				gg_debug_session(sess, GG_DEBUG_MISC, "//   cert issuer: %s\n", buf);
			}

			sess->state = GG_STATE_READING_KEY;
			sess->check = GG_CHECK_READ;
			sess->timeout = GG_DEFAULT_TIMEOUT;

			break;
		}
#endif

		case GG_STATE_READING_KEY:
		{
			struct gg_header *h;
			struct gg_welcome *w;
			struct gg_login70 l;
			unsigned char *password = (unsigned char*) sess->password;
			int ret;

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() GG_STATE_READING_KEY\n");

			memset(&l, 0, sizeof(l));
			l.dunno2 = 0xbe;

			/* XXX bardzo, bardzo, bardzo głupi pomysł na pozbycie
			 * się tekstu wrzucanego przez proxy. */
			if (sess->proxy_addr && sess->proxy_port) {
				char buf[100];

				strcpy(buf, "");
				gg_read_line(sess->fd, buf, sizeof(buf) - 1);
				gg_chomp(buf);
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() proxy response:\n//   %s\n", buf);

				while (strcmp(buf, "")) {
					gg_read_line(sess->fd, buf, sizeof(buf) - 1);
					gg_chomp(buf);
					if (strcmp(buf, ""))
						gg_debug_session(sess, GG_DEBUG_MISC, "//   %s\n", buf);
				}

				/* XXX niech czeka jeszcze raz w tej samej
				 * fazie. głupio, ale działa. */
				sess->proxy_port = 0;

				break;
			}

			/* czytaj pierwszy pakiet. */
			if (!(h = gg_recv_packet(sess))) {
				if (errno == EAGAIN) {
					sess->check = GG_CHECK_READ;
					break;
				}

				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() didn't receive packet (errno=%d, %s)\n", errno, strerror(errno));

				e->type = GG_EVENT_CONN_FAILED;
				e->event.failure = GG_FAILURE_READING;
				sess->state = GG_STATE_IDLE;
				errno2 = errno;
				close(sess->fd);
				errno = errno2;
				sess->fd = -1;
				break;
			}

			if (h->type != GG_WELCOME) {
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() invalid packet received\n");
				free(h);
				close(sess->fd);
				sess->fd = -1;
				errno = EINVAL;
				e->type = GG_EVENT_CONN_FAILED;
				e->event.failure = GG_FAILURE_INVALID;
				sess->state = GG_STATE_IDLE;
				break;
			}

			w = (struct gg_welcome*) ((char*) h + sizeof(struct gg_header));
			w->key = gg_fix32(w->key);

			l.hash_type = sess->hash_type;

			switch (sess->hash_type) {
				case GG_LOGIN_HASH_GG32:
				{
					unsigned int hash;

					hash = gg_login_hash(password, w->key);
					gg_debug_session(sess, GG_DEBUG_DUMP, "// gg_watch_fd() challenge %.4x --> GG32 hash %.8x\n", w->key, hash);
					memcpy(l.hash, &hash, sizeof(hash));

					break;
				}

				case GG_LOGIN_HASH_SHA1:
				{
					char tmp[41];
					int i;

					gg_login_hash_sha1((char*) password, w->key, l.hash);
					for (i = 0; i < 40; i += 2)
						snprintf(tmp + i, sizeof(tmp) - i, "%02x", l.hash[i / 2]);

					gg_debug_session(sess, GG_DEBUG_DUMP, "// gg_watch_fd() challenge %.4x --> SHA1 hash: %s\n", w->key, tmp);

					break;
				}
			}

			free(h);

			free(sess->password);
			sess->password = NULL;

			if (gg_dcc_ip == (unsigned long) inet_addr("255.255.255.255")) {
				struct sockaddr_in sin;
				unsigned int sin_len = sizeof(sin);

				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() detecting address\n");

				if (!getsockname(sess->fd, (struct sockaddr*) &sin, &sin_len)) {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() detected address to %s\n", inet_ntoa(sin.sin_addr));
					l.local_ip = sin.sin_addr.s_addr;
				} else {
					gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() unable to detect address\n");
					l.local_ip = 0;
				}
			} else
				l.local_ip = gg_dcc_ip;

			sess->client_addr = l.local_ip;

			l.uin = gg_fix32(sess->uin);
			l.status = gg_fix32(sess->initial_status ? sess->initial_status : GG_STATUS_AVAIL);
			l.version = gg_fix32(sess->protocol_version);
			l.local_port = gg_fix16(gg_dcc_port);
			l.image_size = sess->image_size;

			if (sess->external_addr && sess->external_port > 1023) {
				l.local_ip = sess->external_addr;
				l.local_port = gg_fix16(sess->external_port);
			}

			gg_debug_session(sess, GG_DEBUG_TRAFFIC, "// gg_watch_fd() sending GG_LOGIN70 packet\n");
			ret = gg_send_packet(sess, GG_LOGIN70, &l, sizeof(l), sess->initial_descr, (sess->initial_descr) ? strlen(sess->initial_descr) : 0, NULL);

			free(sess->initial_descr);
			sess->initial_descr = NULL;

			if (ret == -1) {
				gg_debug_session(sess, GG_DEBUG_TRAFFIC, "// gg_watch_fd() sending packet failed. (errno=%d, %s)\n", errno, strerror(errno));
				errno2 = errno;
				close(sess->fd);
				errno = errno2;
				sess->fd = -1;
				e->type = GG_EVENT_CONN_FAILED;
				e->event.failure = GG_FAILURE_WRITING;
				sess->state = GG_STATE_IDLE;
				break;
			}

			sess->state = GG_STATE_READING_REPLY;
			sess->check = GG_CHECK_READ;

			break;
		}

		case GG_STATE_READING_REPLY:
		{
			struct gg_header *h;

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() GG_STATE_READING_REPLY\n");

			if (!(h = gg_recv_packet(sess))) {
				if (errno == EAGAIN) {
					sess->check = GG_CHECK_READ;
					break;
				}

				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() didn't receive packet (errno=%d, %s)\n", errno, strerror(errno));
				e->type = GG_EVENT_CONN_FAILED;
				e->event.failure = GG_FAILURE_READING;
				sess->state = GG_STATE_IDLE;
				errno2 = errno;
				close(sess->fd);
				errno = errno2;
				sess->fd = -1;
				break;
			}

			if (h->type == GG_LOGIN_OK || h->type == GG_NEED_EMAIL) {
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() login succeded\n");
				e->type = GG_EVENT_CONN_SUCCESS;
				sess->state = GG_STATE_CONNECTED;
				sess->check = GG_CHECK_READ;
				sess->timeout = -1;
				sess->status = (sess->initial_status) ? sess->initial_status : GG_STATUS_AVAIL;
				free(h);
				break;
			}

			if (h->type == GG_LOGIN_FAILED) {
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() login failed\n");
				e->event.failure = GG_FAILURE_PASSWORD;
				errno = EACCES;
			} else if (h->type == GG_DISCONNECTING) {
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() too many incorrect password attempts\n");
				e->event.failure = GG_FAILURE_INTRUDER;
				errno = EACCES;
			} else {
				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() invalid packet\n");
				e->event.failure = GG_FAILURE_INVALID;
				errno = EINVAL;
			}

			e->type = GG_EVENT_CONN_FAILED;
			sess->state = GG_STATE_IDLE;
			errno2 = errno;
			close(sess->fd);
			errno = errno2;
			sess->fd = -1;
			free(h);

			break;
		}

		case GG_STATE_CONNECTED:
		{
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() GG_STATE_CONNECTED\n");

			sess->last_event = time(NULL);

			if ((res = gg_watch_fd_connected(sess, e)) == -1) {

				gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() watch_fd_connected failed (errno=%d, %s)\n", errno, strerror(errno));

 				if (errno == EAGAIN) {
					e->type = GG_EVENT_NONE;
					res = 0;
				} else
					res = -1;
			}

			sess->check = GG_CHECK_READ;

			break;
		}
	}

done:
	if (res == -1) {
		free(e);
		e = NULL;
	} else {
		if (sess->send_buf && (sess->state == GG_STATE_READING_REPLY || sess->state == GG_STATE_CONNECTED))
			sess->check |= GG_CHECK_WRITE;
	}

	return e;

fail_connecting:
	if (sess->fd != -1) {
		errno2 = errno;
		close(sess->fd);
		errno = errno2;
		sess->fd = -1;
	}
	e->type = GG_EVENT_CONN_FAILED;
	e->event.failure = GG_FAILURE_CONNECTING;
	sess->state = GG_STATE_IDLE;
	goto done;

fail_resolving:
	e->type = GG_EVENT_CONN_FAILED;
	e->event.failure = GG_FAILURE_RESOLVING;
	sess->state = GG_STATE_IDLE;
	goto done;

fail_unavailable:
	e->type = GG_EVENT_CONN_FAILED;
	e->event.failure = GG_FAILURE_UNAVAILABLE;
	sess->state = GG_STATE_IDLE;
	goto done;
}

/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 8
 * indent-tabs-mode: notnil
 * End:
 *
 * vim: shiftwidth=8:
 */
