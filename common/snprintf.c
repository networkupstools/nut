/*
 * Copyright Patrick Powell 1995
 * This code is based on code written by Patrick Powell (papowell@astart.com)
 * It may be used for any purpose as long as this notice remains intact
 * on all source code distributions
 */

/**************************************************************
 * Original:
 * Patrick Powell Tue Apr 11 09:48:21 PDT 1995
 * A bombproof version of doprnt (dopr) included.
 * Sigh.  This sort of thing is always nasty do deal with.  Note that
 * the version here does not include floating point...
 *
 * snprintf() is used instead of sprintf() as it does limit checks
 * for string length.  This covers a nasty loophole.
 *
 * The other functions are there to prevent NULL pointers from
 * causing nast effects.
 *
 * More Recently:
 *  Brandon Long <blong@fiction.net> 9/15/96 for mutt 0.43
 *  This was ugly.  It is still ugly.  I opted out of floating point
 *  numbers, but the formatter understands just about everything
 *  from the normal C string format, at least as far as I can tell from
 *  the Solaris 2.5 printf(3S) man page.
 *
 *  Brandon Long <blong@fiction.net> 10/22/97 for mutt 0.87.1
 *    Ok, added some minimal floating point support, which means this
 *    probably requires libm on most operating systems.  Don't yet
 *    support the exponent (e,E) and sigfig (g,G).  Also, fmtint()
 *    was pretty badly broken, it just wasn't being exercised in ways
 *    which showed it, so that's been fixed.  Also, formated the code
 *    to mutt conventions, and removed dead code left over from the
 *    original.  Also, there is now a builtin-test, just compile with:
 *           gcc -DTEST_SNPRINTF -o snprintf snprintf.c -lm
 *    and run snprintf for results.
 *
 *  Thomas Roessler <roessler@guug.de> 01/27/98 for mutt 0.89i
 *    The PGP code was using unsigned hexadecimal formats.
 *    Unfortunately, unsigned formats simply didn't work.
 *
 *  Michael Elkins <me@cs.hmc.edu> 03/05/98 for mutt 0.90.8
 *    The original code assumed that both snprintf() and vsnprintf() were
 *    missing.  Some systems only have snprintf() but not vsnprintf(), so
 *    the code is now broken down under HAVE_SNPRINTF and HAVE_VSNPRINTF.
 *
 *  Andrew Tridgell (tridge@samba.org) Oct 1998
 *    fixed handling of %.0f
 *    added test for HAVE_LONG_DOUBLE
 *
 **************************************************************/

#include "config.h"

#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#if !defined(HAVE_SNPRINTF) || !defined(HAVE_VSNPRINTF)

/* Define this as a fall through, HAVE_STDARG_H is probably already set */

#ifndef HAVE_VARARGS_H
#define HAVE_VARARGS_H
#endif

/* varargs declarations: */

#if defined(HAVE_STDARG_H)
# include <stdarg.h>
# define HAVE_STDARGS    /* let's hope that works everywhere (mj) */
# define VA_LOCAL_DECL   va_list ap
# define VA_START(f)     va_start(ap, f)
# define VA_SHIFT(v,t)  ;   /* no-op for ANSI */
# define VA_END          va_end(ap)
#else
# if defined(HAVE_VARARGS_H)
#  include <varargs.h>
#  undef HAVE_STDARGS
#  define VA_LOCAL_DECL   va_list ap
#  define VA_START(f)     va_start(ap)      /* f is ignored! */
#  define VA_SHIFT(v,t) v = va_arg(ap,t)
#  define VA_END        va_end(ap)
# else
/*XX ** NO VARARGS ** XX*/
# endif
#endif

#ifdef HAVE_LONG_DOUBLE
#define LDOUBLE long double
#else
#define LDOUBLE double
#endif

#ifdef HAVE_LONG_LONG_INT
#define LLONG long long
#else
#define LLONG long
#endif

/*int snprintf (char *str, size_t count, const char *fmt, ...);*/
/*int vsnprintf (char *str, size_t count, const char *fmt, va_list arg);*/

static void dopr (char *buffer, size_t maxlen, const char *format,
                  va_list args);
static void fmtstr (char *buffer, size_t *currlen, size_t maxlen,
                    char *value, int flags, int min, int max);
static void fmtint (char *buffer, size_t *currlen, size_t maxlen,
                    long value, int base, int min, int max, int flags);
static void fmtfp (char *buffer, size_t *currlen, size_t maxlen,
                   LDOUBLE fvalue, int min, int max, int flags);
static void dopr_outch (char *buffer, size_t *currlen, size_t maxlen, char c );

/*
 * dopr(): poor man's version of doprintf
 */

/* format read states */
#define DP_S_DEFAULT 0
#define DP_S_FLAGS   1
#define DP_S_MIN     2
#define DP_S_DOT     3
#define DP_S_MAX     4
#define DP_S_MOD     5
#define DP_S_CONV    6
#define DP_S_DONE    7

/* format flags - Bits */
#define DP_F_MINUS 	(1 << 0)
#define DP_F_PLUS  	(1 << 1)
#define DP_F_SPACE 	(1 << 2)
#define DP_F_NUM   	(1 << 3)
#define DP_F_ZERO  	(1 << 4)
#define DP_F_UP    	(1 << 5)
#define DP_F_UNSIGNED 	(1 << 6)

/* Conversion Flags */
#define DP_C_SHORT   1
/* Note: Originally DP_C_SHORT converted to "short int" types, but modernish
 * (C99+ or even earlier) standards require that the minimal type passed
 * through variadic args '...' is an int, and smaller types are padded up
 * to it - so value shifts in memory and erroneous access crashes can occur
 * if smaller data is accessed blindly. Code below has been fixed to not pass
 * "short int" anymore - it just casts the int to desired smaller type (and
 * so drops the padding bits). */
#define DP_C_LONG    2
#define DP_C_LDOUBLE 3
#define DP_C_LLONG   4

#ifdef C89PLUS
#undef C89PLUS
#endif

#if defined(__STDC__) || defined(__STDC_VERSION__)
/* C89+ and C90+ code respectively */
#define C89PLUS 1
#endif

#define char_to_int(p) ((p)- '0')
#define MAX(p,q) (((p) >= (q)) ? (p) : (q))

static void dopr (char *buffer, size_t maxlen, const char *format, va_list args)
{
  char ch;
  LLONG value;
  LDOUBLE fvalue;
  char *strvalue;
  int min;
  int max;
  int state;
  int flags;
  int cflags;
  size_t currlen;

  state = DP_S_DEFAULT;
  currlen = flags = cflags = min = 0;
  max = -1;
  ch = *format++;

  while (state != DP_S_DONE)
  {
    if ((ch == '\0') || (currlen >= maxlen))
      state = DP_S_DONE;

    switch(state)
    {
    case DP_S_DEFAULT:
      if (ch == '%')
        state = DP_S_FLAGS;
      else
        dopr_outch (buffer, &currlen, maxlen, ch);
      ch = *format++;
      break;
    case DP_S_FLAGS:
      switch (ch)
      {
      case '-':
        flags |= DP_F_MINUS;
        ch = *format++;
        break;
      case '+':
        flags |= DP_F_PLUS;
        ch = *format++;
        break;
      case ' ':
        flags |= DP_F_SPACE;
        ch = *format++;
        break;
      case '#':
        flags |= DP_F_NUM;
        ch = *format++;
        break;
      case '0':
        flags |= DP_F_ZERO;
        ch = *format++;
        break;
      default:
        state = DP_S_MIN;
        break;
      }
      break;
    case DP_S_MIN:
      if (isdigit((unsigned char)ch))
      {
        min = 10*min + char_to_int (ch);
        ch = *format++;
      }
      else if (ch == '*')
      {
        min = va_arg (args, int);
        ch = *format++;
        state = DP_S_DOT;
      }
      else
        state = DP_S_DOT;
      break;
    case DP_S_DOT:
      if (ch == '.')
      {
        state = DP_S_MAX;
        ch = *format++;
      }
      else
        state = DP_S_MOD;
      break;
    case DP_S_MAX:
      if (isdigit((unsigned char)ch))
      {
        if (max < 0)
          max = 0;
        max = 10*max + char_to_int (ch);
        ch = *format++;
      }
      else if (ch == '*')
      {
        max = va_arg (args, int);
        ch = *format++;
        state = DP_S_MOD;
      }
      else
        state = DP_S_MOD;
      break;
    case DP_S_MOD:
      switch (ch)
      {
      case 'h':
        cflags = DP_C_SHORT;
        ch = *format++;
        break;
      case 'l':
        cflags = DP_C_LONG;
        ch = *format++;
        if (ch == 'l') {        /* It's a long long */
          cflags = DP_C_LLONG;
          ch = *format++;
        }
        break;
      case 'L':
        cflags = DP_C_LDOUBLE;
        ch = *format++;
        break;
      default:
        break;
      }
      state = DP_S_CONV;
      break;
    case DP_S_CONV:
      switch (ch)
      {
      case 'd':
      case 'i':
        if (cflags == DP_C_SHORT)
#ifdef C89PLUS
          value = (short int)va_arg (args, int);
#else
          value = va_arg (args, short int);
#endif
        else if (cflags == DP_C_LONG)
          value = va_arg (args, long int);
        else if (cflags == DP_C_LLONG)
          value = va_arg (args, LLONG);
        else
          value = va_arg (args, int);
        fmtint (buffer, &currlen, maxlen, value, 10, min, max, flags);
        break;
      case 'o':
        flags |= DP_F_UNSIGNED;
        if (cflags == DP_C_SHORT)
#ifdef C89PLUS
          value = (unsigned short int)va_arg (args, unsigned int);
#else
          value = va_arg (args, unsigned short int);
#endif
        else if (cflags == DP_C_LONG)
          value = (long)va_arg (args, unsigned long int);
        else if (cflags == DP_C_LLONG)
          value = (long)va_arg (args, unsigned LLONG);
        else
          value = (long)va_arg (args, unsigned int);
        fmtint (buffer, &currlen, maxlen, value, 8, min, max, flags);
        break;
      case 'u':
        flags |= DP_F_UNSIGNED;
        if (cflags == DP_C_SHORT)
#ifdef C89PLUS
          value = (unsigned short int)va_arg (args, unsigned int);
#else
          value = va_arg (args, unsigned short int);
#endif
        else if (cflags == DP_C_LONG)
          value = (long)va_arg (args, unsigned long int);
        else if (cflags == DP_C_LLONG)
          value = (LLONG)va_arg (args, unsigned LLONG);
        else
          value = (long)va_arg (args, unsigned int);
        fmtint (buffer, &currlen, maxlen, value, 10, min, max, flags);
        break;
      case 'X':
        flags |= DP_F_UP;
        goto fallthrough_case_x;
      case 'x':
      fallthrough_case_x:
        flags |= DP_F_UNSIGNED;
        if (cflags == DP_C_SHORT)
#ifdef C89PLUS
          value = (unsigned short int)va_arg (args, unsigned int);
#else
          value = va_arg (args, unsigned short int);
#endif
        else if (cflags == DP_C_LONG)
          value = (long)va_arg (args, unsigned long int);
        else if (cflags == DP_C_LLONG)
          value = (LLONG)va_arg (args, unsigned LLONG);
        else
          value = (long)va_arg (args, unsigned int);
        fmtint (buffer, &currlen, maxlen, value, 16, min, max, flags);
        break;
      case 'f':
        if (cflags == DP_C_LDOUBLE)
          fvalue = va_arg (args, LDOUBLE);
        else
          fvalue = va_arg (args, double);
        /* um, floating point? */
        fmtfp (buffer, &currlen, maxlen, fvalue, min, max, flags);
        break;
      case 'E':
        flags |= DP_F_UP;
        goto fallthrough_case_e;
      case 'e':
      fallthrough_case_e:
        if (cflags == DP_C_LDOUBLE)
          fvalue = va_arg (args, LDOUBLE);
        else
          fvalue = va_arg (args, double);
        break;
      case 'G':
        flags |= DP_F_UP;
        goto fallthrough_case_g;
      case 'g':
      fallthrough_case_g:
        if (cflags == DP_C_LDOUBLE)
          fvalue = va_arg (args, LDOUBLE);
        else
          fvalue = va_arg (args, double);
        break;
      case 'c':
        dopr_outch (buffer, &currlen, maxlen, va_arg (args, int));
        break;
      case 's':
        strvalue = va_arg (args, char *);
        if (max < 0)
          max = maxlen; /* ie, no max */
        fmtstr (buffer, &currlen, maxlen, strvalue, flags, min, max);
        break;
      case 'p':
        strvalue = va_arg (args, void *);
        fmtint (buffer, &currlen, maxlen, (long) strvalue, 16, min, max, flags);
        break;
      case 'n':
        if (cflags == DP_C_SHORT)
        {
          short int *num;
          num = va_arg (args, short int *);
          *num = currlen;
        }
        else if (cflags == DP_C_LONG)
        {
          long int *num;
          num = va_arg (args, long int *);
          *num = (long int)currlen;
        }
        else if (cflags == DP_C_LLONG)
        {
          LLONG *num;
          num = va_arg (args, LLONG *);
          *num = (LLONG)currlen;
        }
        else
        {
          int *num;
          num = va_arg (args, int *);
          *num = currlen;
        }
        break;
      case '%':
        dopr_outch (buffer, &currlen, maxlen, ch);
        break;
      case 'w':
        /* not supported yet, treat as next char */
        ch = *format++;
        break;
      default:
        /* Unknown, skip */
        break;
      }
      ch = *format++;
      state = DP_S_DEFAULT;
      flags = cflags = min = 0;
      max = -1;
      break;
    case DP_S_DONE:
      break;
    default:
      /* hmm? */
      break; /* some picky compilers need this */
    }
  }
  if (currlen < maxlen - 1)
    buffer[currlen] = '\0';
  else
    buffer[maxlen - 1] = '\0';
}

static void fmtstr (char *buffer, size_t *currlen, size_t maxlen,
                    char *value, int flags, int min, int max)
{
  int padlen, strln;     /* amount to pad */
  int cnt = 0;

  if (value == 0)
  {
    value = "<NULL>";
  }

  for (strln = 0; value[strln]; ++strln); /* strlen */
  padlen = min - strln;
  if (padlen < 0)
    padlen = 0;
  if (flags & DP_F_MINUS)
    padlen = -padlen; /* Left Justify */

  while ((padlen > 0) && (cnt < max))
  {
    dopr_outch (buffer, currlen, maxlen, ' ');
    --padlen;
    ++cnt;
  }
  while (*value && (cnt < max))
  {
    dopr_outch (buffer, currlen, maxlen, *value++);
    ++cnt;
  }
  while ((padlen < 0) && (cnt < max))
  {
    dopr_outch (buffer, currlen, maxlen, ' ');
    ++padlen;
    ++cnt;
  }
}

/* Have to handle DP_F_NUM (ie 0x and 0 alternates) */

static void fmtint (char *buffer, size_t *currlen, size_t maxlen,
                    long value, int base, int min, int max, int flags)
{
  int signvalue = 0;
  unsigned long uvalue;
  char convert[20];
  int place = 0;
  int spadlen = 0; /* amount to space pad */
  int zpadlen = 0; /* amount to zero pad */
  int caps = 0;

  if (max < 0)
    max = 0;

  uvalue = value;

  if(!(flags & DP_F_UNSIGNED))
  {
    if( value < 0 ) {
      signvalue = '-';
      uvalue = -value;
    }
    else
      if (flags & DP_F_PLUS)  /* Do a sign (+/i) */
        signvalue = '+';
    else
      if (flags & DP_F_SPACE)
        signvalue = ' ';
  }

  if (flags & DP_F_UP) caps = 1; /* Should characters be upper case? */

  do {
    convert[place++] =
      (caps? "0123456789ABCDEF":"0123456789abcdef")
      [uvalue % (unsigned)base  ];
    uvalue = (uvalue / (unsigned)base );
  } while(uvalue && (place < 20));
  if (place == 20) place--;
  convert[place] = 0;

  zpadlen = max - place;
  spadlen = min - MAX (max, place) - (signvalue ? 1 : 0);
  if (zpadlen < 0) zpadlen = 0;
  if (spadlen < 0) spadlen = 0;
  if (flags & DP_F_ZERO)
  {
    zpadlen = MAX(zpadlen, spadlen);
    spadlen = 0;
  }
  if (flags & DP_F_MINUS)
    spadlen = -spadlen; /* Left Justifty */

#ifdef DEBUG_SNPRINTF
  printf("zpad: %d, spad: %d, min: %d, max: %d, place: %d\n",
         zpadlen, spadlen, min, max, place);
#endif

  /* Spaces */
  while (spadlen > 0)
  {
    dopr_outch (buffer, currlen, maxlen, ' ');
    --spadlen;
  }

  /* Sign */
  if (signvalue)
    dopr_outch (buffer, currlen, maxlen, signvalue);

  /* Zeros */
  if (zpadlen > 0)
  {
    while (zpadlen > 0)
    {
      dopr_outch (buffer, currlen, maxlen, '0');
      --zpadlen;
    }
  }

  /* Digits */
  while (place > 0)
    dopr_outch (buffer, currlen, maxlen, convert[--place]);

  /* Left Justified spaces */
  while (spadlen < 0) {
    dopr_outch (buffer, currlen, maxlen, ' ');
    ++spadlen;
  }
}

#ifndef HAVE_ABS_VAL
static LDOUBLE abs_val (LDOUBLE value)
{
  LDOUBLE result = value;

  if (value < 0)
    result = -value;

  return result;
}
#endif

#ifndef HAVE_FCVT
/* The two routines that may get defined below are only used if we also don't
 * have a fcvt() in the system. Defining and not using the routines may be a
 * warning (fatal with -Werror), so we hide them here. */
# ifndef HAVE_POW10
static LDOUBLE pow10 (int exp)
{
  LDOUBLE result = 1;

  while (exp)
  {
    result *= 10;
    exp--;
  }

  return result;
}
# endif

# ifndef HAVE_ROUND
static long round (LDOUBLE value)
{
  long intpart;

  intpart = (long)value;
  value = value - intpart;
  if (value >= 0.5)
    intpart++;

  return intpart;
}
# endif
#endif /* HAVE_FCVT */

static void fmtfp (char *buffer, size_t *currlen, size_t maxlen,
                   LDOUBLE fvalue, int min, int max, int flags)
{
  int signvalue = 0;
  LDOUBLE ufvalue;
#ifndef HAVE_FCVT
  char iconvert[20];
  char fconvert[20];
#else
  char iconvert[311];
  char fconvert[311];
  char *result;
  int dec_pt, sig;
  int r_length;
# ifdef HAVE_FCVTL
  extern char *fcvtl(long double value, int ndigit, int *decpt, int *sign);
# else
  extern char *fcvt(double value, int ndigit, int *decpt, int *sign);
# endif
#endif
  int iplace = 0;
  int fplace = 0;
  int padlen = 0; /* amount to pad */
  int zpadlen = 0;
#ifndef HAVE_FCVT
  int caps = 0;
  long intpart;
  long fracpart;
#endif

  /*
   * AIX manpage says the default is 0, but Solaris says the default
   * is 6, and sprintf on AIX defaults to 6
   */
  if (max < 0)
    max = 6;

  ufvalue = abs_val (fvalue);

  if (fvalue < 0)
    signvalue = '-';
  else
    if (flags & DP_F_PLUS)  /* Do a sign (+/i) */
      signvalue = '+';
    else
      if (flags & DP_F_SPACE)
        signvalue = ' ';

#if 0
  if (flags & DP_F_UP) caps = 1; /* Should characters be upper case? */
#endif

#ifndef HAVE_FCVT
  intpart = (long)ufvalue;

  /*
   * Sorry, we only support 9 digits past the decimal because of our
   * conversion method
   */
  if (max > 9)
    max = 9;

  /* We "cheat" by converting the fractional part to integer by
   * multiplying by a factor of 10
   */
  fracpart = round ((pow10 (max)) * (ufvalue - intpart));

  if (fracpart >= pow10 (max))
  {
    intpart++;
    fracpart -= pow10 (max);
  }

#ifdef DEBUG_SNPRINTF
  printf("fmtfp: %g %d.%d min=%d max=%d\n",
         (double)fvalue, intpart, fracpart, min, max);
#endif

  /* Convert integer part */
  do {
    iconvert[iplace++] =
      (caps? "0123456789ABCDEF":"0123456789abcdef")[intpart % 10];
    intpart = (intpart / 10);
  } while(intpart && (iplace < 20));
  if (iplace == 20) iplace--;
  iconvert[iplace] = 0;

  /* Convert fractional part */
  do {
    fconvert[fplace++] =
      (caps? "0123456789ABCDEF":"0123456789abcdef")[fracpart % 10];
    fracpart = (fracpart / 10);
  } while(fracpart && (fplace < 20));
  if (fplace == 20) fplace--;
  fconvert[fplace] = 0;
#else  /* use fcvt() */
  if (max > 310)
    max = 310;
# ifdef HAVE_FCVTL
  result = fcvtl(ufvalue, max, &dec_pt, &sig);
# else
  result = fcvt(ufvalue, max, &dec_pt, &sig);
# endif

  r_length = strlen(result);

  /*
   * Fix broken fcvt implementation returns..
   */

  if (r_length == 0)
    {
      result[0] = '0';
      result[1] = '\0';
      r_length = 1;
    }

  if ( r_length < dec_pt )
    dec_pt = r_length;

  if (dec_pt <= 0) {
    iplace = 1;
    iconvert[0] = '0';
    iconvert[1] = '\0';

    fplace = 0;

    while(r_length)
      fconvert[fplace++] = result[--r_length];

    while ((dec_pt < 0) && (fplace < max)) {
                fconvert[fplace++] = '0';
                dec_pt++;
        }
  } else {
    int c;

    iplace=0;
    for(c=dec_pt; c; iconvert[iplace++] = result[--c])
                ;
    iconvert[iplace] = '\0';

    result += dec_pt;
    fplace = 0;

    for(c=(r_length-dec_pt); c; fconvert[fplace++] = result[--c])
                ;
  }
#endif /* fcvt */

  /* -1 for decimal point, another -1 if we are printing a sign */
  padlen = min - iplace - max - 1 - ((signvalue) ? 1 : 0);
  zpadlen = max - fplace;
  if (zpadlen < 0)
    zpadlen = 0;
  if (padlen < 0)
    padlen = 0;
  if (flags & DP_F_MINUS)
    padlen = -padlen; /* Left Justifty */

  if ((flags & DP_F_ZERO) && (padlen > 0))
  {
    if (signvalue)
    {
      dopr_outch (buffer, currlen, maxlen, signvalue);
      --padlen;
      signvalue = 0;
    }
    while (padlen > 0)
    {
      dopr_outch (buffer, currlen, maxlen, '0');
      --padlen;
    }
  }
  while (padlen > 0)
  {
    dopr_outch (buffer, currlen, maxlen, ' ');
    --padlen;
  }
  if (signvalue)
    dopr_outch (buffer, currlen, maxlen, signvalue);

  while (iplace > 0)
    dopr_outch (buffer, currlen, maxlen, iconvert[--iplace]);


#ifdef DEBUG_SNPRINTF
  printf("fmtfp: fplace=%d zpadlen=%d\n", fplace, zpadlen);
#endif

  /*
   * Decimal point.  This should probably use locale to find the correct
   * char to print out.
   */
  if (max > 0) {
          dopr_outch (buffer, currlen, maxlen, '.');

          while (fplace > 0)
                  dopr_outch (buffer, currlen, maxlen, fconvert[--fplace]);
  }

  while (zpadlen > 0)
  {
    dopr_outch (buffer, currlen, maxlen, '0');
    --zpadlen;
  }

  while (padlen < 0)
  {
    dopr_outch (buffer, currlen, maxlen, ' ');
    ++padlen;
  }
}

static void dopr_outch (char *buffer, size_t *currlen, size_t maxlen, char c)
{
  if (*currlen < maxlen)
    buffer[(*currlen)++] = c;
}
#endif /* !defined(HAVE_SNPRINTF) || !defined(HAVE_VSNPRINTF) */

#ifndef HAVE_VSNPRINTF
 int vsnprintf (char *str, size_t count, const char *fmt, va_list args)
{
  str[0] = 0;
  dopr(str, count, fmt, args);
  return(strlen(str));
}
#endif /* !HAVE_VSNPRINTF */

#ifndef HAVE_SNPRINTF
/* VARARGS3 */
#ifdef HAVE_STDARGS
 int snprintf (char *str,size_t count,const char *fmt,...)
#else
 int snprintf (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
  char *str;
  size_t count;
  char *fmt;
#endif
  VA_LOCAL_DECL;

  VA_START (fmt);
  VA_SHIFT (str, char *);
  VA_SHIFT (count, size_t );
  VA_SHIFT (fmt, char *);
  (void) vsnprintf(str, count, fmt, ap);
  VA_END;
  return(strlen(str));
}


#else
 /* keep compilers happy about empty files */
 void dummy_snprintf(void) {}
#endif /* !HAVE_SNPRINTF */

#ifdef TEST_SNPRINTF
#ifndef LONG_STRING
#define LONG_STRING 1024
#endif
 int main (void)
{
  char buf1[LONG_STRING];
  char buf2[LONG_STRING];
  char *fp_fmt[] = {
    "%-1.5f",
    "%1.5f",
    "%123.9f",
    "%10.5f",
    "% 10.5f",
    "%+22.9f",
    "%+4.9f",
    "%01.3f",
    "%4f",
    "%3.1f",
    "%3.2f",
    "%.0f",
    "%.1f",
    NULL
  };
  double fp_nums[] = { -1.5, 134.21, 91340.2, 341.1234, 0203.9, 0.96, 0.996,
    0.9996, 1.996, 4.136, 6442452944.1234, 0};
  char *int_fmt[] = {
    "%-1.5d",
    "%1.5d",
    "%123.9d",
    "%5.5d",
    "%10.5d",
    "% 10.5d",
    "%+22.33d",
    "%01.3d",
    "%4d",
    NULL
  };
  long int_nums[] = { -1, 134, 91340, 341, 0203, 0};
  int x, y;
  int fail = 0;
  int num = 0;

  printf ("Testing snprintf format codes against system sprintf...\n");

  for (x = 0; fp_fmt[x] != NULL ; x++)
    for (y = 0; fp_nums[y] != 0 ; y++)
    {
      snprintf (buf1, sizeof (buf1), fp_fmt[x], fp_nums[y]);
      sprintf (buf2, fp_fmt[x], fp_nums[y]);
      if (strcmp (buf1, buf2))
      {
        printf("snprintf doesn't match Format: %s\n\tsnprintf = %s\n\tsprintf  = %s\n",
            fp_fmt[x], buf1, buf2);
        fail++;
      }
      num++;
    }

  for (x = 0; int_fmt[x] != NULL ; x++)
    for (y = 0; int_nums[y] != 0 ; y++)
    {
      snprintf (buf1, sizeof (buf1), int_fmt[x], int_nums[y]);
      sprintf (buf2, int_fmt[x], int_nums[y]);
      if (strcmp (buf1, buf2))
      {
        printf("snprintf doesn't match Format: %s\n\tsnprintf = %s\n\tsprintf  = %s\n",
            int_fmt[x], buf1, buf2);
        fail++;
      }
      num++;
    }
  printf ("%d tests failed out of %d.\n", fail, num);
}
#endif /* SNPRINTF_TEST */

