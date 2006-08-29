/*
 * $Id$
 *
 * myYahoo Miranda Plugin 
 *
 * Authors: Gennady Feldman (aka Gena01) 
 *          Laurent Marechal (aka Peorth)
 *
 * This code is under GPL and is based on AIM, MSN and Miranda source code.
 * I want to thank Robert Rainwater and George Hazan for their code and support
 * and for answering some of my questions during development of this plugin.
 */
#ifndef _YAHOO_SEARCH_H_
#define _YAHOO_SEARCH_H_

int YahooBasicSearch(WPARAM wParam,LPARAM lParam);
void ext_yahoo_got_search_result(int id, int found, int start, int total, YList *contacts);

#endif
