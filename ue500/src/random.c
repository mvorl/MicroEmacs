/*
 * This file contains the command processing functions for a number of random
 * commands. There is no functional grouping here, for sure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "estruct.h"
#include "eproto.h"
#include "edef.h"
#include "elang.h"


/*
 * Display the current position of the cursor, in origin 1 X-Y coordinates, the
 * character that is under the cursor (in hex), and the fraction of the text
 * that is before the cursor. The displayed column is not the current column,
 * but the column that would be used on an infinite width display. Normally this
 * is bound to "C-X =".
 */

int PASCAL NEAR showcpos(f, n)

int f, n;                               /* prefix flag and argument */

{
    register LINE *lp;                  /* current line */
    register long numchars;             /* # of chars in file */
    register long numlines;             /* # of lines in file */
    register long predchars;            /* # chars preceding point */
    register long predlines;            /* # lines preceding point */
    register int curchar;               /* character under cursor */
    int ratio;
    int col;
    int savepos;                /* temp save for current offset */
    int ecol;                           /* column pos/end of current line */

    /* starting at the beginning of the buffer */
    lp = lforw(curbp->b_linep);
    curchar = '\r';

    /* start counting chars and lines */
    numchars = 0;
    numlines = 0L;
    while ( lp != curbp->b_linep ) {

        /* if we are on the current line, record it */
        if ( lp == curwp->w_dotp ) {
            predlines = numlines;
            predchars = numchars + get_w_doto(curwp);
            if ( (get_w_doto(curwp)) == get_lused(lp) )
                curchar = '\r';
            else
                curchar = lgetc(lp, get_w_doto(curwp));
        }

        /* on to the next line */
        ++numlines;
        numchars += get_lused(lp) + 1;
        lp = lforw(lp);
    }

    /* if at end of file, record it */
    if ( curwp->w_dotp == curbp->b_linep ) {
        predlines = numlines;
        predchars = numchars;
    }

    /* Get real column and end-of-line column. */
    col = getccol(FALSE);
    savepos = get_w_doto(curwp);
    set_w_doto(curwp, get_lused(curwp->w_dotp));
    ecol = getccol(FALSE);
    set_w_doto(curwp, savepos);

    ratio = 0;                          /* Ratio before dot. */
    if ( numchars != 0 )
        ratio = (100L * predchars) / numchars;

    /* summarize and report the info */
#if     DBCS
    if ( is2byte(ltext(curwp->w_dotp), ltext(curwp->w_dotp) + get_w_doto(curwp)) ) {
        mlwrite( TEXT220,
/*      "Line %D/%D Col %d/%d Char %D/%D (%d%%) char = 0x%x%x" */
                 predlines+1, numlines+1, col, ecol, predchars, numchars, ratio,
                 (unsigned char)curchar,
                 (unsigned char)( lgetc(curwp->w_dotp, get_w_doto(curwp)+1) ) );

        return (TRUE);
    }
#endif
    mlwrite(TEXT60,
/*      "Line %D/%D Col %d/%d Char %D/%D (%d%%) char = 0x%x" */
            predlines + 1,
            numlines + 1,
            col,
            ecol,
            predchars,
            numchars,
            ratio,
            curchar);

    return (TRUE);
}

long PASCAL NEAR getlinenum(bp, sline)  /* get the a line number */

BUFFER *bp;                             /* buffer to get current line from */
LINE *sline;                    /* line to search for */
{
    register LINE   *lp;        /* current line */
    register long numlines;             /* # of lines before point */

    /* starting at the beginning of the buffer */
    lp = lforw(bp->b_linep);

    /* start counting lines */
    numlines = 0L;
    while ( lp != bp->b_linep ) {
        /* if we are on the current line, record it */
        if ( lp == sline )
            break;
        ++numlines;
        lp = lforw(lp);
    }

    /* and return the resulting count */
    return (numlines + 1L);
}


/*
 * Return current column.  Stop at first non-blank given TRUE argument.
 */

int PASCAL NEAR getccol(bflg)
int bflg;

{
    int           i     = 0;
    int           col   = 0;
    unsigned char c     = '\0';
    int           doto  = 0;

    doto  = get_w_doto(curwp);
#ifdef JES_REPAIR_CODE
    REPAIR(doto <= get_lused(curwp->w_dotp), doto = get_lused(curwp->w_dotp));
#else
    ASRT  (doto <= get_lused(curwp->w_dotp));
#endif
    for ( i = 0; i < doto; ++i )  {
        c = lgetc(curwp->w_dotp, i) & 0xFF;
        if ( c != ' ' && c != '\t' && bflg )
            break;
        if ( c == '\t' && tabsize > 0 )
            col += -(col % tabsize) + (tabsize - 1);
        else {
            if ( disphigh && c > 0x7f ) {
                col += 2;
                c -= 0x80;
            }
            if ( c < 0x20 || c == 0x7f )
                ++col;
        }
        ++col;
    }

    return (col);
}

/*  findcol: Return display column in line at char position */

int PASCAL NEAR findcol(lp, pos)

LINE * lp;                              /* line to scan */
int pos;                                /* character offset */
{
    register int c, i, col;

    col = 0;
    for ( i = 0; i < pos; ++i ) {
        c = lgetc(lp, i);
        if ( c == '\t' && tabsize > 0 )
            col += -(col % tabsize) + (tabsize - 1);
        else {
            if ( disphigh && c > 0x7f ) {
                col += 2;
                c -= 0x80;
            }
            if ( c < 0x20 || c == 0x7F )
                ++col;
        }
        ++col;
    }

    return (col);
}

/*
 * Set current column.
 */

int PASCAL NEAR setccol(pos)

int pos;                                /* position to set cursor */
{
    register int c;             /* character being scanned */
    register int i;             /* index into current line */
    register int col;           /* current cursor column   */
    register int llen;          /* length of line in bytes */

    col = 0;
    llen = get_lused(curwp->w_dotp);

    /* scan the line until we are at or past the target column */
    for ( i = 0; i < llen; ++i ) {
        /* upon reaching the target, drop out */
        if ( col >= pos )
            break;

        /* advance one character */
        c = lgetc(curwp->w_dotp, i);
        if ( c == '\t' && tabsize > 0 )
            col += -(col % tabsize) + (tabsize - 1);
        else {
            if ( disphigh && c > 0x7f ) {
                col += 2;
                c -= 0x80;
            }
            if ( c < 0x20 || c == 0x7F )
                ++col;
        }
        ++col;
    }

    /* set us at the new position */
    set_w_doto(curwp, i);

    /* and tell whether we made it */
    return (col >= pos);
}

/*
 * Twiddle the two characters on either side of dot. If dot is at the end of the
 * line twiddle the two characters before it. Return with an error if dot is at
 * the beginning of line; it seems to be a bit pointless to make this work. This
 * fixes up a very common typo with a single stroke. Normally bound to "C-T".
 * This always works within a line, so "WFEDIT" is good enough. Backward
 * compatibility forces the save/restore of the cursor position to keep this
 * working as it always has.
 */

int PASCAL NEAR twiddle(f, n)

int f, n;                               /* prefix flag and argument */

{
    register LINE *dotp;                /* shorthand to current line pointer */
    int saved_doto;                     /* restore the cursor afterwards */
    register int cl;                    /* characters to swap! */
    register int cr;

    if ( curbp->b_mode & MDVIEW )       /* don't allow this command if  */
        return ( rdonly() );            /* we are in read only mode */

    /* get the current position */
    saved_doto = get_w_doto(curwp);
    dotp = curwp->w_dotp;

    /* get the 2 chars to swap */
    if ( get_w_doto(curwp) == get_lused(dotp) ) {
        if ( get_w_doto(curwp) <= 0 ) {
            set_w_doto(curwp, saved_doto);

            return (FALSE);
        } else {
            set_w_doto(curwp, get_w_doto(curwp) - 1);
        }
    }
    cr = lgetc(dotp, get_w_doto(curwp));
    if ( get_w_doto(curwp) <= 0 ) {
        set_w_doto(curwp, saved_doto);

        return (FALSE);
    } else {
        set_w_doto(curwp, get_w_doto(curwp) - 1);
    }
    cl = lgetc(dotp, get_w_doto(curwp));

    /* swap the characters */
    obj.obj_char = cl;
    lputc(dotp, get_w_doto(curwp), cr);
    undo_insert(OP_REPC, 1L, obj);
    set_w_doto(curwp, get_w_doto(curwp) + 1);
    obj.obj_char = cr;
    lputc(dotp, get_w_doto(curwp), cl);
    undo_insert(OP_REPC, 1L, obj);

    /* restore the cursor position */
    set_w_doto(curwp, saved_doto);

    /* flag the change */
    lchange(WFEDIT);

    return (TRUE);
}

/*
 * Quote the next character, and insert it into the buffer. All the characters
 * are taken literally, including the newline, which does not then have its line
 * splitting meaning. The character is always read, even if it is inserted 0
 * times, for regularity. Bound to "C-Q". If a mouse action or function key is
 * pressed, its symbolic MicroEMACS name gets inserted!
 */

int PASCAL NEAR quote(f, n)

int f, n;                               /* prefix flag and argument */
{
    register int ec;            /* current extended key fetched     */
    register int c;             /* current ascii key fetched        */
    register int status;        /* return value to hold from linstr */
    char key_name[10];          /* name of a keystroke for quoting  */

    if ( curbp->b_mode & MDVIEW )   /* don't allow this command if  */
        return ( rdonly() );        /* we are in read only mode     */

    ec = get_key();

    /* fail this on a negative argument */
    if ( n < 0 )
        return (FALSE);

    /* nothing to insert . . . blow it off */
    if ( n == 0 )
        return (TRUE);

    /* if this is a mouse event or function key, put its name in */
    if ( (ec & MOUS) || (ec & SPEC) ) {
        cmdstr(ec, key_name);
        while ( n-- ) {
            status = linstr(key_name);
            if ( status != TRUE )
                return (status);
        }

        return (TRUE);
    }

    /* otherwise, just insert the raw character */
    c = ectoc(ec);

    return ( linsert(n, c) );
}

/*
 * Set tab size if given non-default argument (n <> 1).  Otherwise, insert a tab
 * into file.  If given argument, n, of zero, change to hard tabs. If n > 1,
 * simulate tab stop every n-characters using spaces. This has to be done in
 * this slightly funny way because the tab (in ASCII) has been turned into "C-I"
 * (in 10 bit code) already. Bound to "C-I".
 */

int PASCAL NEAR tab(f, n)

int f, n;                               /* prefix flag and argument */
{
    if ( n < 0 )
        return (FALSE);

    if ( n == 0 || n > 1 ) {
        stabsize = n;

        return (TRUE);
    }
    if ( !stabsize )
        return ( linsert(1, '\t') );

    return ( linsert(stabsize - (getccol(FALSE) % stabsize), ' ') );
}

int PASCAL NEAR detab(f, n)     /* change tabs to spaces */

int f, n;                               /* default flag and numeric repeat count
                                         */
{
    register int inc;           /* increment to next line [sgn(n)] */

    if ( curbp->b_mode & MDVIEW )       /* don't allow this command if  */
        return ( rdonly() );            /* we are in read only mode */

    if ( tabsize == 0 )
        return FALSE;

    if ( f == FALSE )
        n = reglines();

    /* loop thru detabbing n lines */
    inc = ( (n > 0) ? 1 : -1 );
    while ( n ) {
        set_w_doto(curwp, 0);           /* start at the beginning */

        /* detab the entire current line */
        while ( get_w_doto(curwp) < get_lused(curwp->w_dotp) ) {
            /* if we have a tab */
            if ( lgetc(curwp->w_dotp, get_w_doto(curwp)) == '\t' ) {
                ldelete(1L, FALSE);
/*              insspace(TRUE, 8 - (get_w_doto(curwp) & 7));*/
                insspace( TRUE, tabsize - (get_w_doto(curwp) % tabsize) );
            }
            forwchar(FALSE, 1);
        }

        /* advance/or back to the next line */
        forwline(TRUE, inc);
        n -= inc;
    }
    set_w_doto(curwp, 0);       /* to the begining of the line */
    thisflag &= ~CFCPCN;        /* flag that this resets the goal column */
    lchange(WFEDIT);            /* yes, we have made at least an edit */

    return (TRUE);
}


int PASCAL NEAR entab(f, n)     /* change spaces to tabs where posible */

int f, n;                               /* default flag and numeric repeat count
                                         */
{
    register int inc;           /* increment to next line [sgn(n)] */
    register int fspace;        /* pointer to first space if in a run */
    register int ccol;          /* current cursor column */
    register char cchar;        /* current character */

    if ( curbp->b_mode & MDVIEW )       /* don't allow this command if  */
        return ( rdonly() );            /* we are in read only mode */

    if ( tabsize == 0 )
        return FALSE;

    if ( f == FALSE )
        n = reglines();

    /* loop thru entabbing n lines */
    inc = ( (n > 0) ? 1 : -1 );
    while ( n ) {
        /* entab the entire current line */

        ccol = set_w_doto(curwp, 0);    /* start at the beginning */
        fspace = -1;

        while ( get_w_doto(curwp) < get_lused(curwp->w_dotp) ) {
            /* see if it is time to compress */
            if ( (fspace >= 0) && (nextab(fspace) <= ccol) )
                if ( ccol - fspace < 2 )
                    fspace = -1;
                else {
                    backchar(TRUE, ccol - fspace);
                    ldelete( (long) (ccol - fspace), FALSE );
                    linsert(1, '\t');
                    fspace = -1;
                }

            /* get the current character */
            cchar = lgetc(curwp->w_dotp, get_w_doto(curwp));

            switch ( cchar ) {
            case '\t':                  /* a tab...count em up (no break here)
                                         *  */
                ldelete(1L, FALSE);
                insspace( TRUE, tabsize - (ccol % tabsize) );

            case ' ':                   /* a space...compress? */
                if ( fspace == -1 )
                    fspace = ccol;
                break;

            default:                    /* any other char...just count */
                fspace = -1;
                break;
            }
            ccol++;
            forwchar(FALSE, 1);
        }

        /* advance/or back to the next line */
        forwline(TRUE, inc);
        n -= inc;
        set_w_doto(curwp, 0);   /* start at the beginning */
    }
    set_w_doto(curwp, 0);       /* to the begining of the line */
    thisflag &= ~CFCPCN;        /* flag that this resets the goal column */
    lchange(WFEDIT);            /* yes, we have made at least an edit */

    return (TRUE);
}

/* trim:    trim trailing whitespace from the point to eol with no arguments, it
 * trims the current region
 */

int PASCAL NEAR trim(f, n)

int f, n;                               /* default flag and numeric repeat count
                                         */
{
    register LINE *lp;          /* current line pointer */
    register int offset;        /* original line offset position */
    register int length;        /* current length */
    register int inc;           /* increment to next line [sgn(n)] */

    if ( curbp->b_mode & MDVIEW )       /* don't allow this command if  */
        return ( rdonly() );            /* we are in read only mode */

    if ( f == FALSE )
        n = reglines();

    /* loop thru trimming n lines */
    inc = ( (n > 0) ? 1 : -1 );
    while ( n ) {
        lp = curwp->w_dotp;             /* find current line text */
        offset = get_w_doto(curwp);     /* save original offset */
        length = get_lused(lp);         /* find current length */

        /* trim the current line */
        while ( length > offset ) {
            if ( lgetc(lp, length-1) != ' ' &&lgetc(lp, length-1) != '\t' )
                break;
            length--;
        }
        set_lused(lp, length);

        /* advance/or back to the next line */
        forwline(TRUE, inc);
        n -= inc;
    }
    lchange(WFEDIT);
    thisflag &= ~CFCPCN;        /* flag that this resets the goal column */

    return (TRUE);
}

/*
 * Open up some blank space. The basic plan is to insert a bunch of newlines,
 * and then back up over them. Everything is done by the subcommand procerssors.
 * They even handle the looping. Normally this is bound to "C-O".
 */

int PASCAL NEAR openline(f, n)

int f, n;                               /* prefix flag and argument */
{
    register int i;
    register int s;

    if ( curbp->b_mode & MDVIEW )       /* don't allow this command if  */
        return ( rdonly() );            /* we are in read only mode */

    if ( n < 0 )
        return (FALSE);

    if ( n == 0 )
        return (TRUE);

    i = n;                              /* Insert newlines. */
    do {
        s = lnewline();
    } while (s == TRUE && --i);
    if ( s == TRUE )            /* Then back up overtop */
        s = backchar(f, n);             /* of them all.     */

    return (s);
}

/*
 * Insert a newline. Bound to "C-M". If we are in CMODE, do automatic
 * indentation as specified.
 */

int PASCAL NEAR newline(f, n)

int f, n;                               /* prefix flag and argument */
{
    register int s;

    if ( curbp->b_mode & MDVIEW )       /* don't allow this command if  */
        return ( rdonly() );            /* we are in read only mode */

    if ( n < 0 )
        return (FALSE);

    /* if we are in C mode and this is a default <NL> */
    if ( n == 1 && (curbp->b_mode & MDCMOD) &&curwp->w_dotp != curbp->b_linep )
        return ( cinsert() );

    /*
     * If a newline was typed, fill column is defined, the argument is non-
     * negative, wrap mode is enabled, and we are now past fill column, and we
     * are not read-only, perform word wrap.
     */
    if ( (curwp->w_bufp->b_mode & MDWRAP) && fillcol > 0 &&
         getccol(FALSE) > fillcol &&(curwp->w_bufp->b_mode & MDVIEW) == FALSE )
        execkey(&wraphook, FALSE, 1);

    /* insert some lines */
    while ( n-- ) {
        if ( ( s = lnewline() ) != TRUE )
            return (s);
    }

    return (TRUE);
}

int PASCAL NEAR cinsert()       /* insert a newline and indentation for C */
{
    register char *cptr;        /* string pointer into text to copy */
    register int i;             /* index into line to copy indent from */
    register int llen;          /* length of line to copy indent from */
    register int bracef;        /* was there a brace at the end of line? */
    register LINE *lp;          /* current line pointer */
    register int offset;
    char ichar[NSTRING];        /* buffer to hold indent of last line */

    /* trim the whitespace before the point */
    lp = curwp->w_dotp;
    offset = get_w_doto(curwp);
    while ( offset > 0 &&
            (lgetc(lp, offset - 1) == ' ' ||lgetc(lp, offset - 1) == '\t') ) {
        backdel(FALSE, 1);
        offset--;
    }

    /* check for a brace */
    bracef = (offset > 0 && lgetc(lp, offset - 1) == '{');

    /* put in the newline */
    if ( lnewline() == FALSE )
        return (FALSE);

    /* if the new line is not blank... don't indent it! */
    lp = curwp->w_dotp;
    if ( get_lused(lp) != 0 )
        return (TRUE);

    /* hunt for the last non-blank line to get indentation from */
    while ( get_lused(lp) == 0 && lp != curbp->b_linep )
        lp = lback(lp);

    /* grab a pointer to text to copy indentation from */
    cptr = ltext(lp);
    llen = get_lused(lp);

    /* save the indent of the last non blank line */
    i = 0;
    while ( (i < llen) && (cptr[i] == ' ' || cptr[i] == '\t')&&
            (i < NSTRING - 1) ) {
        ichar[i] = cptr[i];
        ++i;
    }
    ichar[i] = 0;               /* terminate it */

    /* insert this saved indentation */
    linstr(ichar);

    /* and one more tab for a brace */
    if ( bracef )
        tab(FALSE, 1);

    return (TRUE);
}

int PASCAL NEAR insbrace(n, c)  /* insert a brace into the text here...we are in
                                 * CMODE */

int n;                                  /* repeat count */
int c;                                  /* brace to insert (always } for now) */
{
    register int ch;            /* last character before input */
    register int oc;            /* caractere oppose a c */
    register int i, count;
    register int target;        /* column brace should go after */
    register LINE *oldlp;
    register int oldoff;

    /* if we aren't at the beginning of the line... */
    if ( get_w_doto(curwp) != 0 )

        /* scan to see if all space before this is white space */
        for ( i = get_w_doto(curwp) - 1; i >= 0; --i ) {
            ch = lgetc(curwp->w_dotp, i);
            if ( ch != ' ' && ch != '\t' )
                return ( linsert(n, c) );
        }

    /* chercher le caractere oppose correspondant */
    switch ( c ) {
    case '}':
        oc = '{';
        break;

    case ']':
        oc = '[';
        break;

    case ')':
        oc = '(';
        break;

    default:
        return (FALSE);
    }

    oldlp = curwp->w_dotp;
    oldoff = get_w_doto(curwp);

    count = 1;
    backchar(FALSE, 1);

    while ( count > 0 ) {
        if ( get_w_doto(curwp) == get_lused(curwp->w_dotp) )
            ch = '\r';
        else
            ch = lgetc(curwp->w_dotp, get_w_doto(curwp));

        if ( ch == c )
            ++count;
        if ( ch == oc )
            --count;

        backchar(FALSE, 1);
        if ( boundry(curwp->w_dotp, get_w_doto(curwp), REVERSE) )
            break;
    }

    if ( count != 0 ) {                         /* no match */
        curwp->w_dotp = oldlp;
        set_w_doto(curwp, oldoff);

        return ( linsert(n, c) );
    }

    set_w_doto(curwp, 0);   /* debut de ligne */
    /* aller au debut de la ligne apres la tabulation */
    while ( ( ch = lgetc(curwp->w_dotp, get_w_doto(curwp)) ) == ' ' || ch == '\t' )
        forwchar(FALSE, 1);

    /* delete back first */
    target = getccol(FALSE);            /* c'est l'indent que l'on doit avoir */
    curwp->w_dotp = oldlp;
    set_w_doto(curwp, oldoff);

    while ( target != getccol(FALSE) ) {
        if ( target < getccol(FALSE) )          /* on doit detruire des
                                                 * caracteres */
            while ( getccol(FALSE) > target )
                backdel(FALSE, 1);

        else {                          /* on doit en inserer */
            if ( tabsize > 0 )
                while ( target - getccol(FALSE) >= tabsize )
                    linsert(1, '\t');

            linsert(target - getccol(FALSE), ' ');
        }
    }

    /* and insert the required brace(s) */
    return ( linsert(n, c) );
}

int PASCAL NEAR inspound()      /* insert a # into the text here...we are in
                                 * CMODE */
{
    register int ch;            /* last character before input */
    register int i;

    /* if we are at the beginning of the line, no go */
    if ( get_w_doto(curwp) == 0 )
        return ( linsert(1, '#') );

    /* scan to see if all space before this is white space */
    for ( i = get_w_doto(curwp) - 1; i >= 0; --i ) {
        ch = lgetc(curwp->w_dotp, i);
        if ( ch != ' ' && ch != '\t' )
            return ( linsert(1, '#') );
    }

    /* delete back first */
    while ( getccol(FALSE) >= 1 )
        backdel(FALSE, 1);

    /* and insert the required pound */
    return ( linsert(1, '#') );
}

/*
 * Delete blank lines around dot. What this command does depends if dot is
 * sitting on a blank line. If dot is sitting on a blank line, this command
 * deletes all the blank lines above and below the current line. If it is
 * sitting on a non blank line then it deletes all of the blank lines after the
 * line. Normally this command is bound to "C-X C-O". Any argument is ignored.
 */

int PASCAL NEAR deblank(f, n)

int f, n;                               /* prefix flag and argument */
{
    register LINE   *lp1;
    register LINE   *lp2;
    long nld;

    if ( curbp->b_mode & MDVIEW )       /* don't allow this command if  */
        return ( rdonly() );            /* we are in read only mode */

    lp1 = curwp->w_dotp;
    while ( get_lused(lp1) == 0 && ( lp2 = lback(lp1) ) != curbp->b_linep )
        lp1 = lp2;
    lp2 = lp1;
    nld = 0;
    while ( ( lp2 = lforw(lp2) ) != curbp->b_linep && get_lused(lp2) == 0 )
        ++nld;
    if ( nld == 0 )
        return (TRUE);

    curwp->w_dotp = lforw(lp1);
    set_w_doto(curwp, 0);

    return ( ldelete(nld, FALSE) );
}

/*
 * Insert a newline, then enough tabs and spaces to duplicate the indentation of
 * the previous line. Tabs are every tabsize characters. Quite simple. Figure
 * out the indentation of the current line. Insert a newline by calling the
 * standard routine. Insert the indentation by inserting the right number of
 * tabs and spaces. Return TRUE if all ok. Return FALSE if one of the subcomands
 * failed. Normally bound to "C-J".
 */

int PASCAL NEAR indent(f, n)

int f, n;                               /* prefix flag and argument */
{
    register int nicol;
    register int c;
    register int i;

    if ( curbp->b_mode & MDVIEW )       /* don't allow this command if  */
        return ( rdonly() );            /* we are in read only mode */

    if ( n < 0 )
        return (FALSE);

    while ( n-- ) {
        nicol = 0;
        for ( i = 0; i < get_lused(curwp->w_dotp); ++i ) {
            c = lgetc(curwp->w_dotp, i);
            if ( c != ' ' && c != '\t' )
                break;
            if ( c == '\t' )
                if ( tabsize > 0 )
                    nicol += -(nicol % tabsize) + (tabsize - 1);
                else
                    break;

            ++nicol;
        }
        if ( lnewline() == FALSE )
            return FALSE;

        if ( tabsize == 0 ) {
            if ( linsert(nicol, ' ') == FALSE )
                return (FALSE);
        } else if ( ( (i = nicol / tabsize) != 0 &&
                      linsert(i, '\t') == FALSE )||
                    ( (i = nicol % tabsize) != 0 && linsert(i, ' ') == FALSE ) )
            return (FALSE);
    }

    return (TRUE);
}

/*
 * Delete forward. This is real easy, because the basic delete routine does all
 * of the work. Watches for negative arguments, and does the right thing. If any
 * argument is present, it kills rather than deletes, to prevent loss of text if
 * typed with a big argument. Normally bound to "C-D".
 */

int PASCAL NEAR forwdel(f, n)

int f, n;                               /* prefix flag and argument */

{
    /* Don't allow this in read-only mode */
    if ( curbp->b_mode & MDVIEW )
        return ( rdonly() );

    /* with a negative argument, this is a backwards delete */
    if ( n < 0 )
        return ( backdel(f, -n) );

    /* with an argument, flag this to go to the kill buffer! */
    if ( f != FALSE ) {
        if ( (lastflag & CFKILL) == 0 )
            next_kill();
        thisflag |= CFKILL;
    }

    return ( ldelete( (long)n, f ) );
}

/*
 * Delete backwards. This is quite easy too, because it's all done with other
 * functions. Just move the cursor back, and delete forwards. Like delete
 * forward, this actually does a kill if presented with an argument. Bound to
 * both "RUBOUT" and "C-H".
 */

int PASCAL NEAR backdel(f, n)

int f, n;       /* prefix flag and argument */

{
    register int status;

    /* Don't do this command in read-only mode */
    if ( curbp->b_mode & MDVIEW )
        return ( rdonly() );

    /* with a negative argument, this becomes a delete forward */
    if ( n < 0 )
        return ( forwdel(f, -n) );

    /* with an argument, flag this to go to the kill buffer! */
    if ( f != FALSE ) {
        if ( (lastflag & CFKILL) == 0 )
            next_kill();
        thisflag |= CFKILL;
    }

    /* make sure the cursor gets back to the right place on an undo */
    undo_insert(OP_CPOS, 0L, obj);

    /* and now delete the characters */
    if ( ( status = backchar(f, n) ) == TRUE )
        status = ldelete( (long) n, f );

    return (status);
}

/*
 * Kill text. If called without an argument, it kills from dot to the end of the
 * line, unless it is at the end of the line, when it kills the newline. If
 * called with an argument of 0, it kills from the start of the line to dot. If
 * called with a positive argument, it kills from dot forward over that number
 * of newlines. If called with a negative argument it kills backwards that
 * number of newlines. Normally bound to "C-K".
 */

int PASCAL NEAR killtext(f, n)

int f, n;       /* prefix flag and argument */

{
    register LINE *nextp;
    long chunk;

    /* Don't do this command in read-only mode */
    if ( curbp->b_mode & MDVIEW )
        return ( rdonly() );

    /* flag this as a kill */
    if ( (lastflag & CFKILL) == 0 )
        next_kill();
    thisflag |= CFKILL;

    /* make sure the cursor gets back to the right place on an undo */
    undo_insert(OP_CPOS, 0L, obj);

    if ( f == FALSE ) {
        chunk = get_lused(curwp->w_dotp) - get_w_doto(curwp);
        if ( chunk == 0 )
            chunk = 1;
    } else if ( n == 0 ) {
        chunk = (-1) * get_w_doto(curwp);
    } else if ( n > 0 ) {
        chunk = get_lused(curwp->w_dotp) - get_w_doto(curwp) + 1;
        nextp = lforw(curwp->w_dotp);
        while ( --n ) {
            if ( nextp == curbp->b_linep )
                return (FALSE);

            chunk += get_lused(nextp) + 1;
            nextp = lforw(nextp);
        }
    } else if ( n < 0 ) {
        chunk = (-1) * get_w_doto(curwp);
        nextp = lback(curwp->w_dotp);
        while ( n++ ) {
            if ( nextp == curbp->b_linep )
                return (FALSE);

            chunk -= get_lused(nextp) + 1;
            nextp = lback(nextp);
        }
    }

    return ( ldelete(chunk, TRUE) );
}

int PASCAL NEAR setmod(f, n)    /* prompt and set an editor mode */

int f, n;                               /* default and argument */
{
    return ( adjustmode(TRUE, FALSE) );
}

int PASCAL NEAR delmode(f, n)   /* prompt and delete an editor mode */

int f, n;                               /* default and argument */
{
    return ( adjustmode(FALSE, FALSE) );
}

int PASCAL NEAR setgmode(f, n)  /* prompt and set a global editor mode */

int f, n;                               /* default and argument */
{
    return ( adjustmode(TRUE, TRUE) );
}

int PASCAL NEAR delgmode(f, n)  /* prompt and delete a global editor mode */

int f, n;                               /* default and argument */
{
    return ( adjustmode(FALSE, TRUE) );
}

int PASCAL NEAR adjustmode(kind, global)        /* change the editor mode status
                                                 */

int kind;                               /* true = set,      false = delete */
int global;                             /* true = global flag,  false = current
                                         * buffer flag */
{
    register char *scan;        /* scanning pointer to convert prompt */
    register int i;             /* loop index */
    register int status;        /* error return on input */
#if     COLOR
    register int uflag;         /* was modename uppercase?  */
#endif
    char prompt[50];            /* string to prompt user with */
    char cbuf[NPAT];            /* buffer to recieve mode name into */

    /* build the proper prompt string */
    if ( global )
        xstrcpy(prompt, TEXT62);
/*                "Global mode to " */
    else
        xstrcpy(prompt, TEXT63);
/*                "Mode to " */

    if ( kind == TRUE )
        strcat(prompt, TEXT64);
/*                 "add: " */
    else
        strcat(prompt, TEXT65);
/*                 "delete: " */

    /* prompt the user and get an answer */

    status = mlreply(prompt, cbuf, NPAT - 1);
    if ( status != TRUE )
        return (status);

    /* make it uppercase */

    scan = cbuf;
#if     COLOR
    uflag = (*scan >= 'A' && *scan <= 'Z');
#endif
    while ( *scan )
        uppercase( (unsigned char *) scan++ );

    /* test it first against the colors we know */
    if ( ( i = lookup_color(cbuf) ) != -1 ) {

#if     COLOR
        /* finding the match, we set the color */
        if ( global ) {
            if ( uflag ) {
                gfcolor = i;
                TRC( ("Globally setting foreground color to %s/%d",
                      (char *)cbuf, (int)i) );
            } else {
                gbcolor = i;
                TRC( ("Globally setting background color to %s/%d",
                      (char *)cbuf, (int)i) );
            }
# if     WINDOW_TEXT & 0
            refresh_screen(first_screen);
# endif
        } else if ( uflag ) {
            curwp->w_fcolor = i;
            TRC( ("Locally setting foreground color to %s/%d", (char *)cbuf,
                  (int)i) );
        } else {
            curwp->w_bcolor = i;
            TRC( ("Locally setting background color to %s/%d", (char *)cbuf,
                  (int)i) );
        }

        curwp->w_flag |= WFCOLR;
#endif
        mlerase();

        return (TRUE);
    }

    /* test it against the modes we know */

    for ( i = 0; i < NUMMODES; i++ ) {
        if ( strcmp(cbuf, modename[i]) == 0 ) {
            /* finding a match, we process it */
            if ( kind == TRUE )
                if ( global ) {
                    gmode |= (1 << i);
                    if ( (1 << i) == MDOVER )
                        gmode &= ~MDREPL;
                    else if ( (1 << i) == MDREPL )
                        gmode &= ~MDOVER;
                } else {
                    curbp->b_mode |= (1 << i);
                    if ( (1 << i) == MDOVER )
                        curbp->b_mode &= ~MDREPL;
                    else if ( (1 << i) == MDREPL )
                        curbp->b_mode &= ~MDOVER;
                }
            else if ( global )
                gmode &= ~(1 << i);
            else
                curbp->b_mode &= ~(1 << i);
            /* display new mode line */
            if ( global == 0 )
                upmode();
            mlerase();                  /* erase the junk */

            return (TRUE);
        }
    }

    mlwrite(TEXT66);

/*      "No such mode!" */
    return (FALSE);
}

/*  This function simply clears the message line, mainly for macro usage            */

int PASCAL NEAR clrmes(f, n)

int f, n;                               /* arguments ignored */
{
    mlforce("");

    return (TRUE);
}

/*  This function writes a string on the message line mainly for macro usage            */

int PASCAL NEAR writemsg(f, n)

int f, n;                               /* arguments ignored */
{
    register int status;
    char buf[NPAT];             /* buffer to recieve message into */

    if ( ( status = mlreply(TEXT67, buf, NPAT - 1) ) != TRUE )
/*                "Message to write: " */
        return (status);

    /* write the message out */
    mlforce(buf);

    return (TRUE);
}

/*  the cursor is moved to a matching fence */

int PASCAL NEAR getfence(f, n)

int f, n;                               /* not used */
{
    register LINE *oldlp;       /* original line pointer */
    register int oldoff;        /* and offset */
    register int sdir;          /* direction of search (1/-1) */
    register int count;         /* current fence level count */
    register char ch;           /* fence type to match against */
    register char ofence;       /* open fence */
    register char c;            /* current character in scan */
    register int qlevel;        /* quote level */

    /* save the original cursor position */
    oldlp = curwp->w_dotp;
    oldoff = get_w_doto(curwp);

    /* get the current character */
    if ( oldoff == get_lused(oldlp) )
        ch = '\r';
    else
        ch = lgetc(oldlp, oldoff);

    /* setup proper matching fence */
    switch ( ch ) {
    case '(':
        ofence = ')';
        sdir = FORWARD;
        break;

    case '{':
        ofence = '}';
        sdir = FORWARD;
        break;

    case '[':
        ofence = ']';
        sdir = FORWARD;
        break;

    case ')':
        ofence = '(';
        sdir = REVERSE;
        break;

    case '}':
        ofence = '{';
        sdir = REVERSE;
        break;

    case ']':
        ofence = '[';
        sdir = REVERSE;
        break;

    default:
        TTbeep();

        return (FALSE);
    }

    /* set up for scan */
    count = 1;
    qlevel = 0;

    /* scan until we find it, or reach the end of file */
    while ( count > 0 ) {
        if ( sdir == FORWARD )
            forwchar(FALSE, 1);
        else
            backchar(FALSE, 1);

        if ( get_w_doto(curwp) == get_lused(curwp->w_dotp) )
            c = '\r';
        else
            c = lgetc(curwp->w_dotp, get_w_doto(curwp));

        /* these only happen when we are outside quotes */
        if ( (oquote == 0) || (qlevel == 0) ) {

            if ( c == ch )
                ++count;
            if ( c == ofence )
                --count;
        }

        /* if we are quoting.... */
        if ( oquote != 0 ) {
            if ( oquote == cquote ) {

                /* only one quote character */
                if ( c == oquote )
                    qlevel = 1 - qlevel;
            } else {
                /* track opens and closes */
                if ( c == oquote )
                    ++qlevel;
                if ( c == cquote )
                    --qlevel;
            }
        }

        /* at the end/beginning of the buffer.... abort */
        if ( boundry(curwp->w_dotp, get_w_doto(curwp), sdir) )
            break;
    }

    /* if count is zero, we have a match, move the sucker */
    if ( count == 0 ) {
        curwp->w_flag |= WFMOVE;

        return (TRUE);
    }

    /* restore the current position */
    curwp->w_dotp = oldlp;
    set_w_doto(curwp, oldoff);
    TTbeep();

    return (FALSE);
}

/*  Close fences are matched against their partners, and if on screen the cursor
 * briefly lights there     */

#if     PROTO
int PASCAL NEAR fmatch(char ch)
#else
int PASCAL NEAR fmatch(ch)

char ch;        /* fence type to match against */
#endif

{
    register LINE *oldlp;       /* original line pointer */
    register int oldoff;        /* and offset */
    register LINE *toplp;       /* top line in current window */
    register int count;         /* current fence level count */
    register char opench;       /* open fence */
    register char c;            /* current character in scan */
    register int qlevel;        /* quote level */
    register int i;

    /* first get the display update out there */
    update(FALSE);

    /* save the original cursor position */
    oldlp = curwp->w_dotp;
    oldoff = get_w_doto(curwp);

    /* setup proper open fence for passed close fence */
    if ( ch == ')' )
        opench = '(';
    else if ( ch == '}' )
        opench = '{';
    else
        opench = '[';

    /* find the top line and set up for scan */
    toplp = lback(curwp->w_linep);
    count = 1;
    qlevel = 0;
    backchar(FALSE, 1);

    /* scan back until we find it, or reach past the top of the window */
    while ( count > 0 && curwp->w_dotp != toplp ) {

        backchar(FALSE, 1);
        if ( get_w_doto(curwp) == get_lused(curwp->w_dotp) )
            c = '\r';
        else
            c = lgetc(curwp->w_dotp, get_w_doto(curwp));

        /* these only happen when we are outside quotes */
        if ( (oquote == 0) || (qlevel == 0) ) {

            if ( c == ch )
                ++count;
            if ( c == opench )
                --count;
        }

        /* if we are quoting.... */
        if ( oquote != 0 ) {
            if ( oquote == cquote ) {

                /* only one quote character */
                if ( c == oquote )
                    qlevel = 1 - qlevel;
            } else {
                /* track opens and closes */
                if ( c == oquote )
                    ++qlevel;
                if ( c == cquote )
                    --qlevel;
            }
        }

        /* stop at the beginning of the buffer */
        if ( curwp->w_dotp == lforw(curwp->w_bufp->b_linep) &&
             get_w_doto(curwp) == 0 )
            break;
    }

    /* if count is zero, we have a match, display the sucker */
    /* there is a real machine dependant timing problem here we have yet to
     * solve......... */
    if ( count == 0 ) {
#if     WINDOW_MSWIN
        update(FALSE);
        term.t_sleep (term.t_pause);
#else
        for ( i = 0; i < term.t_pause; i++ )
            update(FALSE);
#endif
    }

    /* restore the current position */
    curwp->w_dotp = oldlp;
    set_w_doto(curwp, oldoff);

    return (TRUE);
}

/* ask for and insert a string into the current buffer at the current point */

int PASCAL NEAR istring(f, n)

int f, n;                               /* ignored arguments */

{
    register int status;        /* status return code */
    char tstring[NPAT + 1];     /* string to add */

    /* ask for string to insert */
    status = nextarg(TEXT68, tstring, NPAT, sterm);
/*            "String to insert: " */
    if ( status != TRUE )
        return (status);

    if ( f == FALSE )
        n = 1;

    if ( n < 0 )
        n = -n;

    /* insert it */
    while ( n-- && ( status = linstr(tstring) ) )
        ;

    return (status);
}

int PASCAL NEAR ovstring(f, n)  /* ask for and overwite a string into the
                                 * current buffer at the current point */

int f, n;                               /* ignored arguments */
{
    register int status;        /* status return code */
    char tstring[NPAT + 1];     /* string to add */

    /* ask for string to insert */
    status = nextarg(TEXT69, tstring, NPAT, sterm);
/*            "String to overwrite: " */
    if ( status != TRUE )
        return (status);

    if ( f == FALSE )
        n = 1;

    if ( n < 0 )
        n = -n;

    /* insert it */
    while ( n-- && ( status = lover(tstring) ) )
        ;

    return (status);
}

int PASCAL NEAR lookup_color(sp)

char *sp;                               /* name to look up */
{
    register int i;             /* index into color list */

    /* test it against the colors we know */
    for ( i = 0; i < NCOLORS; i++ ) {
        if ( strcmp(sp, cname[i]) == 0 )
            return (i);
    }

    return (-1);
}


/*====================================================================*/


char *xstrcpy(char *s1, CONST char *s2) /* strcpy() possibly overlapping regions
                                         */
{
    char  *s  = NULL;

    ASRT(NULL != s1);
    ASRT(NULL != s2);

    ASRT(NULL != (s = (char *)calloc(strlen(s2) + 1, sizeof (char))));
    strcpy(s, s2);
    strcpy(s1, s);
    FREE(s);

    return s1;
}

char *xstrncpy(char *s1, CONST char *s2, int n) /* strncpy() possibly
                                                 * overlapping regions */
{
    int   l2  = 0;
    char *s   = NULL;

    ASRT(0 <= n);
    if ( 0 == n ) {
        return s1;
    }
    ASRT(NULL != s1);
    ASRT(NULL != s2);

    l2  = strlen(s2);
    l2  = MAX2(l2, n);
    ASRT(NULL !=(s  = (char *)calloc( l2 + 1, sizeof (char) )));
    strncpy(s, s2, n);  /* This will always succedd and result in
                         * a '\0'-terminated s. */

    strncpy(s1, s, n);
    FREE(s);

    return s1;
}

/* An unelegant but portable version of C99 vsnprintf():                */
/* May be called with NULL == s .AND. 0 == n to get the size that would */
/* be the result of an unrestricted write.                              */
/* Returns the number of characters (not including the trailing '\0')   */
/* that would have been written if n were large enough.                 */
int xvsnprintf(char *s, size_t n, CONST char *fmt, va_list ap)
{
    int nn  = n;                /* Want a signed value */
    int rc  = 0;
    int nr  = 0;                /* Number of chars to read */
    static FILE *fp = NULL;

    ASRT(0    <= nn);
    ASRT(NULL != fmt);

    if ( NULL == fp ) {           /* One-time initialization */
        if ( NULL == ( fp = tmpfile() ) ) { /* ANSI C: Opened in wb+ mode */
            return (-1);
        }
        /*
         * Buffering: No real IO for not too large junks
         */
        if ( 0 != setvbuf(fp, NULL, _IOFBF, 0) ) {
            return (-2);
        }
    }

    rewind(fp);
    if ( 0 > ( rc = vfprintf(fp, fmt, ap) ) ) {
        return (-3);
    }

    if ( 0 == nn ) {
        /*
         * EMPTY
         */
    } else if ( 1 == nn ) {
        ASRT(NULL != s);

        s[0] = '\0';
    } else {
        ASRT(NULL != s);

        if ( 0 == rc ) {
            s[0] = '\0';
        } else {                /* Read back characters written: */
            rewind(fp);
            nr = MIN2(rc, nn - 1);
            if ( 1 != fread(s, nr, 1, fp) ) {
                return (-4);
            }
            s[nr] = '\0';
        }
    }

    if ( 0 < nn ) {
        TRC(("xvsnprintf: %s", s));
    }

    return rc;
}

/* Like the C99 snprintf():                                             */
/* May be called with NULL == s .AND. 0 == n to get the size that would */
/* be the result of an unrestricted write.                              */
/* Returns the number of characters (not including the trailing '\0')   */
/* that would have been written if n were large enough.                 */
int xsnprintf(char *s, size_t n, CONST char *fmt, ...)
{
    int     rc  = 0;
    va_list ap;

    ZEROMEM(ap);
    ASRT(NULL != fmt);

    va_start(ap, fmt);
    rc = xvsnprintf(s, n, fmt, ap);
    va_end(ap);

    return rc;
}

/* Like GNU C vasprintf:                                        */
/* Allocate (using malloc()) a string large enough to hold the  */
/* resulting string.                                            */
int xvasprintf(char **ret, const char *fmt, va_list ap)
{
    int     rc  = 0;
    int     len = 0;
    char    *cp = NULL;
    va_list aq;

    ZEROMEM(aq);
    VA_COPY(aq, ap);
    ASRT(NULL != ret);
    ASRT(NULL != fmt);

    if ( 0 > (len = xvsnprintf(NULL, 0, fmt, ap)) ) {
        *ret  = NULL;

        return len;
    }
    len += 1;
    ASRT(NULL != (cp = calloc(len, sizeof(char))));

    if ( 0 > (rc  = xvsnprintf(cp, len, fmt, aq)) ) {
        FREE(cp);
    }
    VA_END(aq);
    *ret  = cp;

    return rc;
}

/* Like GNU C asprintf:                                         */
/* Allocate (using malloc()) a string large enough to hold the  */
/* resulting string.                                            */
int xasprintf(char **ret, const char *fmt, ...)
{
    int     rc  = 0;
    va_list ap;

    ZEROMEM(ap);
    ASRT(NULL != ret);
    ASRT(NULL != fmt);

    va_start(ap, fmt);
    rc = xvasprintf(ret, fmt, ap);
    va_end(ap);

    return rc;
}

char *xstrdup(CONST char *str)
{
    char  *res  = NULL;
    int   len   = 0;


    if ( NULL == str ) {
        return NULL;
    }
    len = strlen(str);
    ASRT(NULL != (res = (char *)calloc( len + 1, sizeof (char) )));
    strcpy(res, str);

    return res;
}

/* Concatenate character c to string str and malloc the result. */
/* Input string must either be NULL or malloced.                */
char *astrcatc(const char *str, const char c)
{
    char  *res  = NULL;
    char  *nstr = NULL;
    int   len   = 0;

    if ( NULL == str ) {
        len = 1 + 1;
        ASRT(NULL != (nstr = (char *)calloc(len, sizeof(char))));
    } else {
        len = strlen(str) + 1 + 1;
        ASRT(NULL != (nstr = (char *)realloc(str, len * sizeof(char))));
    }
    nstr[len - 2]  = c;
    nstr[len - 1]  = '\0';

    return nstr;
}

/* Concatenate string d to string str and malloc the result.    */
/* Input string must either be NULL or malloced.                */
char *astrcat(const char *str, const char *s)
{
    char  *res      = NULL;
    char  *nstr     = NULL;
    int   len       = 0;
    const char  *xs = (NULL == s)? "" : s;
    int   slen      = strlen(xs);

    if ( NULL == str ) {
        len = slen + 1;
        ASRT(NULL != (nstr = (char *)calloc(len, sizeof(char))));
        strcpy(nstr, xs);
    } else {
        len = strlen(str) + slen + 1;
        ASRT(NULL != (nstr = (char *)realloc(str, len * sizeof(char))));
        strcat(nstr, xs);
    }

    return nstr;
}


/*====================================================================*/


FILE  *GetTrcFP(void)
{
  static int  FirstCall = !0;
  static FILE *TrcFP    = NULL;

  if ( FirstCall )  {
    FirstCall = 0;

    {
      char *fname  = getenv(TRC_FILE_ENVVAR);

      if ( NULL == fname ) {
        TrcFP = NULL;
      } else {
        if ( NULL != (TrcFP = fopen(fname, "a")) )  {
          setbuf(TrcFP, NULL);
        } else {
          TrcFP = stderr;
        }
      }
    }
  }

  return TrcFP;
}

int          DebugMessage_lnno_   = 0;
CONST char  *DebugMessage_fname_  = (CONST char *)"";
int DebugMessage(CONST char *fmt, ...)
{
    int     rc    = 0;
    va_list ap;
    FILE    *TFP  = GetTrcFP();

    ZEROMEM(ap);

    if ( TFP )  {
      fprintf(TFP, "%s (%s/%03d): ", "TRC", DebugMessage_fname_,
              DebugMessage_lnno_);
      va_start(ap, fmt);
      rc = vfprintf(TFP, fmt, ap);
      va_end(ap);
      fprintf(TFP, "%s", "\n");
      fflush(TFP);
    }

    return rc;
}


/*====================================================================*/


char lputc_(LINE *lp, int n, char c, const char *fnam, int lno)
{
    ASRTK(NULL != lp,                 fnam, lno);
    ASRTK(lp->l_used_ <= lp->l_size_, fnam, lno);
    ASRTK(0 <= n,                     fnam, lno);
    ASRTK(n < lp->l_size_,            fnam, lno);

    return ( lp->l_text_[n] = c );
}

#undef  FUNC_
#define FUNC_ lgetc_
#if ( IS_UNIX() )
unsigned char FUNC_(LINE *lp, int n, const char *fnam, int lno)
#else
         char FUNC_(LINE *lp, int n, const char *fnam, int lno)
#endif
{
    ASRTK(NULL != lp,                     fnam, lno);
    ASRTK(lp->l_used_ <= lp->l_size_,     fnam, lno);
    ASRTK((0 <= n) && (n <= lp->l_used_), fnam, lno);

    if ( n == lp->l_used_ ) {
        TRCK(("%s(): Read at Buffer Boundry: l_size_ = %d, l_used_ = %d, l_text_[%d] = '%c'",
              FSTR_, (int)lp->l_size_, n, n, lp->l_text_[n]), fnam, lno);

        if ( n == lp->l_size_ ) {
            return ( '\0' );
        } else {
            return ( lp->l_text_[n] );
        }
    }

    return ( lp->l_text_[n] );
}

#undef  FUNC_
#define FUNC_ lgetcp_
char *FUNC_(LINE *lp, int n, const char *fnam, int lno)
{
    ASRTK(NULL != lp,                     fnam, lno);
    ASRTK(lp->l_used_ <= lp->l_size_,     fnam, lno);
    ASRTK((0 <= n) && (n <= lp->l_used_), fnam, lno);

    if ( n == lp->l_used_ ) {
        TRCK(("%s(): Read at Buffer Boundry: l_size_ = %d, l_used_ = %d, l_text_[%d] = '%c'",
              FSTR_, (int)lp->l_size_, n, n, lp->l_text_[n]), fnam, lno);
    }

    return ( &(lp->l_text_[n]) );
}

int get_lused_(LINE *lp, const char *fnam, int lno)
{
    ASRTK(NULL != lp,                 fnam, lno);
    ASRTK(lp->l_used_ <= lp->l_size_, fnam, lno);

    return ( (lp)->l_used_ );
}

int set_lused_(LINE *lp, int used, const char *fnam, int lno)
{
    ASRTK(NULL != lp,                 fnam, lno);
    ASRTK(lp->l_used_ <= lp->l_size_, fnam, lno);
    ASRTK(0 <= used,                  fnam, lno);
    ASRTK(used <= lp->l_size_,        fnam, lno);

    return ( (lp)->l_used_ = used );
}

int get_lsize_(LINE *lp, const char *fnam, int lno)
{
    ASRTK(NULL != lp,                 fnam, lno);
    ASRTK(lp->l_used_ <= lp->l_size_, fnam, lno);

    return ( (lp)->l_size_ );
}

#undef  FUNC_
#define FUNC_ get_w_doto_
int FUNC_(EWINDOW *wp, const char *fnam, int lno)
{
    ASRTK(NULL != wp,                                 fnam, lno);
    ASRTK(NULL != wp->w_dotp,                         fnam, lno);
    ASRTK(0 <= wp->w_dotp->l_used_,                   fnam, lno);
    ASRTK(wp->w_dotp->l_used_ <= wp->w_dotp->l_size_, fnam, lno);
    ASRTK(0 <= wp->w_doto_,                           fnam, lno);

    if ( wp->w_dotp->l_used_ < wp->w_doto_ )  {
        TRCK(("%s(): w_dotp->l_used_(%d) < wp->w_doto_(%d)",
              FSTR_, wp->w_dotp->l_used_, wp->w_doto_), fnam, lno);
    }

    return ( wp->w_doto_ );
}

#undef  FUNC_
#define FUNC_ set_w_doto_
int FUNC_(EWINDOW *wp, int doto, const char *fnam, int lno)
{
    ASRTK(NULL != wp,                                 fnam, lno);
    ASRTK(NULL != wp->w_dotp,                         fnam, lno);
    ASRTK(0 <= wp->w_dotp->l_used_,                   fnam, lno);
    ASRTK(wp->w_dotp->l_used_ <= wp->w_dotp->l_size_, fnam, lno);
    ASRTK(0 <= wp->w_doto_,                           fnam, lno);

    if ( 0 > doto ) {
        TRCK(("%s(): Negativ doto: %d. REWRITTEN TO %d.",
              FSTR_, doto, 0), fnam, lno);

        doto  = 0;
    }
    if ( wp->w_dotp->l_used_ < doto ) {
        TRCK(("%s(): Too large doto: %d. REWRITTEN TO: %d.",
              FSTR_, doto, wp->w_dotp->l_used_), fnam, lno);
        doto  = wp->w_dotp->l_used_;  /* TODO: Use `l_used_ - 1'? */
    }

    return ( (wp)->w_doto_ = doto );
}

#undef  FUNC_
#define FUNC_ get_b_doto_
int FUNC_(BUFFER *bp, const char *fnam, int lno)
{
    ASRTK(NULL != bp,                                 fnam, lno);
    ASRTK(NULL != bp->b_dotp,                         fnam, lno);
    ASRTK(0 <= bp->b_dotp->l_used_,                   fnam, lno);
    ASRTK(bp->b_dotp->l_used_ <= bp->b_dotp->l_size_, fnam, lno);
    ASRTK(0 <= bp->b_doto_,                           fnam, lno);

    if ( bp->b_dotp->l_used_ < bp->b_doto_ )  {
        TRCK(("%s(): b_dotp->l_used_(%d) < bp->b_doto_(%d)",
              FSTR_, bp->b_dotp->l_used_, bp->b_doto_), fnam, lno);
    }

    return ( (bp)->b_doto_ );
}

#undef  FUNC_
#define FUNC_ set_b_doto_
int FUNC_(BUFFER *bp, int doto, const char *fnam, int lno)
{
    ASRTK(NULL != bp,                                 fnam, lno);
    ASRTK(NULL != bp->b_dotp,                         fnam, lno);
    ASRTK(0 <= bp->b_dotp->l_used_,                   fnam, lno);
    ASRTK(bp->b_dotp->l_used_ <= bp->b_dotp->l_size_, fnam, lno);
    ASRTK(0 <= bp->b_doto_,                           fnam, lno);

    if ( 0 > doto ) {
        TRCK(("%s(): Negativ doto: %d. REWRITTEN TO %d.",
              FSTR_, doto, 0), fnam, lno);

        doto  = 0;
    }
    if ( bp->b_dotp->l_used_ < doto ) {
        TRCK(("%s(): Too large doto: %d. REWRITTEN TO: %d.",
              FSTR_, doto, bp->b_dotp->l_used_), fnam, lno);
        doto  = bp->b_dotp->l_used_;  /* TODO: Use `l_used_ - 1'? */
    }

    return ( (bp)->b_doto_ = doto );
}


/*====================================================================*/

int TransformRegion(filter_func_T filter, void *argp)
{
    LINE    *linep  = NULL;
    int     loffs   = 0;
    long    rsize   = 0;
    int     s       = 0;
    char    *rtext  = xstrdup("");
    char    *rstart = xstrdup("");
    int     i       = 0;
    REGION  region;

    ZEROMEM(region);
    ASRT(NULL != filter);

    if ( ( s = getregion(&region) ) != TRUE ) {
        return FALSE;
    }

    linep = region.r_linep;                     /* Current line.    */
    loffs = region.r_offset;                    /* Current offset.  */
    rsize = region.r_size;
    for (  i = 0; i < loffs; i++ )  {
        rstart  = astrcatc(rstart, lgetc(linep, i));
    }
    while ( rsize-- ) {
        if ( loffs == get_lused(linep) ) {      /* End of line.         */
            rtext = astrcatc(rtext, '\r');
            linep = lforw(linep);
            loffs = 0;
        } else {                                /* Middle of line.  */
            rtext = astrcatc(rtext, lgetc(linep, loffs));
            ++loffs;
        }
    }

    /* make sure the cursor gets back to the right place on an undo */
    undo_insert(OP_CPOS, 0L, obj);

    /* delete the region */
    curwp->w_dotp = region.r_linep;
    set_w_doto(curwp, region.r_offset);

    if ( TRUE != ldelete(region.r_size, TRUE) ) {
        FREE(rstart);
        FREE(rtext);

        return FALSE;
    }


    /*===============================================================*/
    /* Do something with rtext here.                                 */
    /*===============================================================*/
    {
        char  *outline  = (*filter)(rstart, rtext, argp);
        FREE(rstart);
        FREE(rtext);
        linstr(outline);
        FREE(outline);
    }

    return TRUE;
}


static char  *filter_test(const char *rstart, const char *rtext, void *argp)
{
    char  *res  = NULL;
    int   rc    = 0;
    char  *str  = NULL;
    int   i     = 0;
    char  c     = '\0';

    ASRT(NULL != rstart);
    ASRT(NULL != rtext);

    str = xstrdup(rstart);
    for ( i = 0; '\0' != (c = str[i]); i++ )  {
        str[i]  = toupper(c);
    }

    rc  = xasprintf(&res, "<%s><%s>", str, rtext);
    FREE(str);

    return res;
}

int PASCAL NEAR tr_region_test(f, n)

int f, n;                               /* ignored arguments */

{
    /*===============================================================*/
    /* Don't do this command in read-only mode */
    if ( curbp->b_mode&MDVIEW ) {
        return ( rdonly() );
    }

    /* flag this command as a kill */
    if ( (lastflag & CFKILL) == 0 ) {
        next_kill();
    }
    thisflag |= CFKILL;
    /*===============================================================*/

    return TransformRegion(&filter_test, NULL);
}


/*====================================================================*/



/**********************************************************************/
/* EOF                                                                */
/**********************************************************************/
