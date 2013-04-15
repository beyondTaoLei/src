/* Plane-wave spray. */
/*
  Copyright (C) 2004 University of Texas at Austin
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <rsf.h>
#include "predict.h"

int main (int argc, char *argv[])
{
    bool verb;
    int n1,n2,n3, i1,i2,i3, is, ns, ns2, ip, order;
    float eps, ***u, **p, *trace;
    sf_file inp, out, dip;

    sf_init(argc,argv);
    inp = sf_input("in");
    dip = sf_input("dip");
    out = sf_output("out");

    if (!sf_histint(dip,"n1",&n1)) sf_error("No n1= in input");
    if (!sf_histint(dip,"n2",&n2)) sf_error("No n2= in input");
    n3 = sf_leftsize(dip,2);

    if (!sf_getbool("verb",&verb)) verb=false;
    /* verbosity */
    if (!sf_getfloat("eps",&eps)) eps=0.01;
    /* regularization */
    
    if (!sf_getint("ns",&ns)) sf_error("Need ns=");
    /* spray radius */
    ns2 = 2*ns+1;

    if (!sf_getint("order",&order)) order=1;
    /* accuracy order */

    sf_putint(out,"n2",ns2);
    sf_putfloat(out,"o2",-ns);
    sf_putfloat(out,"d2",1.0);
    sf_shiftdim(inp, out, 2);

    predict_init (n1, n2, eps*eps, order, 1, false);

    u = sf_floatalloc3(n1,ns2,n2);
    for (i2=0; i2 < n2; i2++) {
	for (is=0; is < ns2; is++) {
	    for (i1=0; i1 < n1; i1++) {
		u[i2][is][i1] = 0.;
	    }
	}
    }

    p = sf_floatalloc2(n1,n2);
    trace = sf_floatalloc(n1);

    for (i3=0; i3 < n3; i3++) {
	if (verb) fprintf(stderr,"cmp %d of %d\n",i3+1,n3);
	sf_floatread(p[0],n1*n2,dip);

	for (i2=0; i2 < n2; i2++) { /* loop over traces */
	    sf_floatread(trace,n1,inp);

	    for (i1=0; i1 < n1; i1++) {
		u[i2][ns][i1] = trace[i1];
	    }

	    /* predict forward */
	    for (is=0; is < ns; is++) {
		ip = i2-is-1;
		if (ip < 0) break;
		predict_step(false,false,trace,p[ip]);
		for (i1=0; i1 < n1; i1++) {
		    u[ip][ns-is-1][i1] = trace[i1];
		}
	    }

	    for (i1=0; i1 < n1; i1++) {
		trace[i1] = u[i2][ns][i1];
	    }

	    /* predict backward */
	    for (is=0; is < ns; is++) {
		ip = i2+is+1;
		if (ip >= n2) break;
		predict_step(false,true,trace,p[ip-1]);
		for (i1=0; i1 < n1; i1++) {
		    u[ip][ns+is+1][i1] = trace[i1];
		}
	    }	    
	}
	sf_floatwrite(u[0][0],n1*ns2*n2,out);
    }	    

    exit (0);
}

/* 	$Id: Mflat.c 1131 2005-04-20 18:19:10Z fomels $	 */