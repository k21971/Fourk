/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2017-07-15 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef PERMONST_H
# define PERMONST_H

# include "global.h"

enum mon_extypes {
    MX_NONE = 0,
    MX_EPRI,
    MX_EMIN,
    MX_EDOG,
    MX_ESHK,
    MX_EGD
};

/* This structure covers all attack forms.
 * aatyp is the gross attack type (eg. claw, bite, breath, ...)
 * adtyp is the damage type (eg. physical, fire, cold, spell, ...)
 * damn is the number of hit dice of damage from the attack.
 * damd is the number of sides on each die.
 *
 * Some attacks can do no points of damage.  Additionally, some can
 * have special effects *and* do damage as well.  If damn and damd
 * are set, they may have a special meaning.  For example, if set
 * for a blinding attack, they determine the amount of time blinded.
 */

struct attack {
    char struct_type;  /* Should always be 'a' for this struct.
                          See doc/struct_types.txt for the list. */
    uchar aatyp;
    uchar adtyp, damn, damd;
};

/* Max # of attacks for any given monster.
 */

# define NATTK    6

/* Weight of a human body
 */

# define WT_HUMAN 1450

# ifndef ALIGN_H
#  include "align.h"
# endif
# include "monattk.h"
# include "monflag.h"

struct permonst {
    char struct_type;  /* Should always be 'P' for this struct.
                           See doc/struct_types.txt for the list. */
    const char *mname;  /* full name */
    char mlet;  /* symbol */
    schar mlevel,       /* base monster level */
          difficulty,   /* monster difficulty, formerly monstr/mstrength */
          mmove,        /* move speed */
          ac,   /* (base) armor class */
          stealth, /* (base) stealthiness */
          mr;   /* (base) magic resistance */
    aligntyp maligntyp; /* basic monster alignment */
    unsigned short geno;        /* creation/geno mask value */
    struct attack mattk[NATTK]; /* attacks matrix */
    unsigned short cwt, /* weight of corpse */
          cnutrit;      /* its nutritional value */
    short pxtyp;        /* type of extension */
    uchar msound;       /* noise it makes (6 bits) */
    uchar msize;        /* physical size (3 bits) */
    unsigned int mflagsr; /* race flags (formerly in mflags2) */
    unsigned int mflags1, /* boolean bitflags */
        mflags2, mflags3; /* more boolean bitflags */
    unsigned int mskill;        /* proficiency bitflags */
    uchar mresists;     /* resistances */
    uchar mconveys;     /* conveyed by eating */
    uchar mcolor;       /* color to use */
};

extern const struct permonst mons[];    /* the master list of monster types */

# define VERY_SLOW 3
# define SLOW_SPEED 9
# define NORMAL_SPEED 12/* movement rates */
# define FAST_SPEED 15
# define VERY_FAST 24

# define NON_PM     (-1)              /* "not a monster" */
# define LOW_PM     (NON_PM+1)        /* first monster in mons[] */
# define SPECIAL_PM PM_LONG_WORM_TAIL /* [normal] < ~ < [special] */
        /* mons[SPECIAL_PM] through mons[NUMMONS-1], inclusive, are never
           generated randomly and cannot be polymorphed into */

#endif /* PERMONST_H */

