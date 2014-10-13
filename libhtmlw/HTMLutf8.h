/* Changes for Mosaic-CK (C) 2009, 2010 Cameron Kaiser 
   Eventually this is going to need to be fully Unicode. -- ck */

/****************************************************************************
 * NCSA Mosaic for the X Window System                                      *
 * Software Development Group                                               *
 * National Center for Supercomputing Applications                          *
 * University of Illinois at Urbana-Champaign                               *
 * 605 E. Springfield, Champaign IL 61820                                   *
 * mosaic@ncsa.uiuc.edu                                                     *
 *                                                                          *
 * Copyright (C) 1993, Board of Trustees of the University of Illinois      *
 *                                                                          *
 * NCSA Mosaic software, both binary and source (hereafter, Software) is    *
 * copyrighted by The Board of Trustees of the University of Illinois       *
 * (UI), and ownership remains with the UI.                                 *
 *                                                                          *
 * The UI grants you (hereafter, Licensee) a license to use the Software    *
 * for academic, research and internal business purposes only, without a    *
 * fee.  Licensee may distribute the binary and source code (if released)   *
 * to third parties provided that the copyright notice and this statement   *
 * appears on all copies and that no charge is associated with such         *
 * copies.                                                                  *
 *                                                                          *
 * Licensee may make derivative works.  However, if Licensee distributes    *
 * any derivative work based on or derived from the Software, then          *
 * Licensee will (1) notify NCSA regarding its distribution of the          *
 * derivative work, and (2) clearly notify users that such derivative       *
 * work is a modified version and not the original NCSA Mosaic              *
 * distributed by the UI.                                                   *
 *                                                                          *
 * Any Licensee wishing to make commercial use of the Software should       *
 * contact the UI, c/o NCSA, to negotiate an appropriate license for such   *
 * commercial use.  Commercial use includes (1) integration of all or       *
 * part of the source code into a product for sale or license by or on      *
 * behalf of Licensee to third parties, or (2) distribution of the binary   *
 * code or source code to third parties that need it to utilize a           *
 * commercial product sold or licensed by or on behalf of Licensee.         *
 *                                                                          *
 * UI MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THIS SOFTWARE FOR   *
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED          *
 * WARRANTY.  THE UI SHALL NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY THE    *
 * USERS OF THIS SOFTWARE.                                                  *
 *                                                                          *
 * By using or copying this Software, Licensee agrees to abide by the       *
 * copyright law and all other applicable laws of the U.S. including, but   *
 * not limited to, export control laws, and the terms of this license.      *
 * UI shall have the right to terminate this license immediately by         *
 * written notice upon Licensee's breach of, or non-compliance with, any    *
 * of its terms.  Licensee may be held legally responsible for any          *
 * copyright infringement that is caused or encouraged by Licensee's        *
 * failure to abide by the terms of this license.                           *
 *                                                                          *
 * Comments and questions are welcome and can be sent to                    *
 * mosaic-x@ncsa.uiuc.edu.                                                  *
 ****************************************************************************/

/* Place the UTF-8 sequence below, along with the character to map it to.
   This is very simple-minded, but it works surprisingly well for many sites.
   A nice way to get more characters to encode is from

	http://netzreport.googlepages.com/online_tool_for_url_en_decoding.html

*/

typedef struct utf8_esc_rec {
	char *tag;
	char value;
} UTF8Esc;

/* For performance, put common sequences first. */
static UTF8Esc UTF8Escapes[] = {
	/* spaces */
	{"\xc2\xa0", ' '},

	/* nbsp */
	{"\xe2\x80\x8e", ' '},

	/* quotes */
	{"\xe2\x80\x98", '\''},
	{"\xe2\x80\x99", '\''},
	{"\xe2\x80\x9c", '"'}, 
	{"\xe2\x80\x9d", '"'}, 

	/* dashes */
	{"\xe2\x80\x93", '-'}, /* en dash */
	{"\xe2\x80\x94", '-'}, /* em dash */

	/* dot dot dot -- this is obviously imprecise */
	{"\xe2\x80\xa6", '\267'},

	/* bullets */
	{"\xc2\xb7", '*'},
	{"\xe2\x80\xa2", '*'},

	/* Latin diacritics */
	{"\xc3\xa9", '\351'}, /* eacute */
	{"\xc3\xbc", '\374'}, /* uuml */
	{"\xc5\x8d", '\364'}, /* o macron -> ocirc */
	{"\xc5\x8f", 'o'}, /* hmm. should be o with a reverse hacek */

	/* miscellaneous */
	{"\xe2\x86\xa9", '<'}, /* curly arrow */
	{"\xc3\x97", 'x'}, /* multiplication sign */

	{NULL, '\0'},
};

