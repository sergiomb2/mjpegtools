/*
 *  Copyright (C) 2001 Kawamata/Hitoshi <hitoshi.kawamata@nifty.ne.jp>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "yuvfilters.h"

extern const YfTaskClass_t yuvstdin;
extern const YfTaskClass_t yuvstdout;
extern const YfTaskClass_t FILTER;

int verbose = 1;

static void
usage(char **argv)
{
  mjpeg_error("Usage: %s %s", argv[0], (*FILTER.usage)());
}

int
main(int argc, char **argv)
{
  YfTaskCore_t *h, *hreader;
  int ret;
  char *p;

  if (1 < argc && (!strcmp(argv[1], "-?") ||
		   !strcmp(argv[1], "-h") ||
		   !strcmp(argv[1], "--help"))) {
    usage(argv);
    return 0;
  }
  if ((p = getenv("MJPEG_VERBOSITY")))
    verbose = atoi(p);

   y4m_accept_extensions(1);

  ret = 1;
  if (!(hreader = YfAddNewTask(&yuvstdin, argc, argv, NULL)))
    goto RETURN;
  if (!YfAddNewTask(&FILTER, argc, argv, hreader))
    goto FINI;
  if (!YfAddNewTask(&yuvstdout, argc, argv, hreader))
    goto FINI;

  ret = (*yuvstdin.frame)(hreader, NULL, NULL);
  if (ret == Y4M_ERR_EOF)
    ret = Y4M_OK;
  if (ret != Y4M_OK)
    mjpeg_error("%s", y4m_strerr(ret));

 FINI:
  for (h = hreader; h; h = hreader) {
    hreader = h->handle_outgoing;
    (*h->method->fini)(h);
  }
 RETURN:
  return ret;
}
