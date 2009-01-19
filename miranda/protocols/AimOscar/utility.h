/*
Plugin of Miranda IM for communicating with users of the AIM protocol.
Copyright (c) 2008-2009 Boris Krasnovskiy
Copyright (C) 2005-2006 Aaron Myles Landwehr

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef UTILITY_H
#define UTILITY_H

char *normalize_name(const char *s);
char* lowercase_name(char* s);
char* trim_name(const char* s);
char* trim_str(char* s);
void create_group(const char *group);
void set_extra_icon(HANDLE hContact, HANDLE hImage, int column_type);
unsigned int aim_oft_checksum_file(char *filename);
void long_ip_to_char_ip(unsigned long host, char* ip);
unsigned long char_ip_to_long_ip(char* ip);
bool cap_cmp(const char* cap,const char* cap2);
bool is_oscarj_ver_cap(char* cap);
bool is_aimoscar_ver_cap(char* cap);
bool is_kopete_ver_cap(char* cap);
bool is_qip_ver_cap(char* cap);
bool is_micq_ver_cap(char* cap);
bool is_im2_ver_cap(char* cap);
bool is_sim_ver_cap(char* cap);
bool is_naim_ver_cap(char* cap);
bool is_digsby_ver_cap(char* cap);
unsigned short get_random(void);

template <class T>
T* renew(T* src, int size, int size_chg)
{
	T* dest=new T[size+size_chg];
	memcpy(dest,src,size*sizeof(T));
	delete[] src;
	return dest;
}

struct BdListItem
{
    char* name;
    unsigned short item_id;

    BdListItem() { name = NULL; item_id = 0; }
    BdListItem(const char* snt, unsigned short id) { name = strldup(snt); item_id = id; }
    ~BdListItem() { if (name) delete[] name; }
};

struct BdList : public OBJLIST<BdListItem>
{
    BdList() : OBJLIST<BdListItem>(5) {}

    void add(const char* snt, unsigned short id)
    { insert(new BdListItem(snt, id)); }

    unsigned short add(const char* snt)
    { 
        unsigned short id = get_free_id();
        insert(new BdListItem(snt, id));
        return id;
    }

    unsigned short get_free_id(void);
    unsigned short find_id(const char* name);
    char* find_name(unsigned short id);
    void remove_by_id(unsigned short id);
};

#endif
