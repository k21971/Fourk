/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-13 */
/* Copyright (C) 1987, 1988, 1989 by Ken Arromdee */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * Polymorph self routine.
 *
 * Note: the light source handling code assumes that youmonst.m_id will always
 * remain 0 when it handles the case of the player polymorphed into a
 * light-emitting monster.
 */

#include "hack.h"

static void polyman(const char *, const char *);
static void break_armor(boolean);
static void drop_weapon(int, boolean);
static void uunstick(void);
static int armor_to_dragon(struct obj *, struct obj *);
static void newman(void);

static boolean inventory_provides_polyform_ability(struct obj *,
                                                   struct polyform_ability *,
                                                   struct attack *);
static int dobreathe(const struct nh_cmd_arg *);
static int dospit(const struct nh_cmd_arg *);
static int doremove(void);
static int dospinweb(void);
static int dosummon(void);
static int dogaze(void);
static int dohide(void);
static int domindblast(void);


/* update the youmonst.data structure pointer */
void
set_uasmon(void)
{
    const struct permonst *pm = &mons[u.umonnum];

    if (!Upolyd)
        pm = u.ufemale ? &pm_you_female : &pm_you_male;
    set_mon_data(&youmonst, pm, 0);
}

/* make a (new) human out of the player */
static void
polyman(const char *fmt, const char *arg)
{
    boolean sticky = sticks(youmonst.data) && u.ustuck &&
        !Engulfed;
    boolean could_pass_walls = Passes_walls;
    boolean was_blind = ! !Blind;

    if (Upolyd) {
        u.acurr = u.macurr;     /* restore old attribs */
        u.amax = u.mamax;
        u.umonnum = u.umonster;
        u.ufemale = u.mfemale;
    }
    set_uasmon();

    u.mh = u.mhmax = 0;
    u.mtimedone = 0;
    u.uundetected = 0;

    if (sticky)
        uunstick();
    cancel_mimicking("");

    newsym(u.ux, u.uy);

    /* check whether player foolishly genocided self while poly'd */
    if ((mvitals[urole.malenum].mvflags & G_GENOD) ||
        (urole.femalenum != NON_PM &&
         (mvitals[urole.femalenum].mvflags & G_GENOD)) ||
        (mvitals[urace.malenum].mvflags & G_GENOD) ||
        (urace.femalenum != NON_PM &&
         (mvitals[urace.femalenum].mvflags & G_GENOD))) {
        pline(msgc_fatal_predone, "As you return to %s form, you die!",
              urace.adj);
        done(GENOCIDED, delayed_killer(GENOCIDED));
    } else
        pline(msgc_statusend, fmt, arg);

    if (u.twoweap && !could_twoweap(youmonst.data))
        untwoweapon();

    if (u.utraptype == TT_PIT) {
        if (could_pass_walls) { /* player forms cannot pass walls */
            u.utrap = rn1(6, 2);
        }
    }
    if (was_blind && !Blind) {  /* reverting from eyeless */
        Blinded = 1L;
        make_blinded(0L, TRUE); /* remove blindness */
    }

    if (!Levitation && !u.ustuck &&
        (is_pool(level, u.ux, u.uy) || is_lava(level, u.ux, u.uy)))
        spoteffects(TRUE);

    see_monsters(FALSE);
    update_supernatural_abilities();

    if (!uarmg) {
        const char *kbuf = msgprintf("returning to %s form while wielding",
                                     urace.adj);
        selftouch("No longer petrify-resistant, you", kbuf);
    }
}

void
change_sex(void)
{
    /* setting u.umonster for caveman/cavewoman or priest/priestess swap
       unintentionally makes `Upolyd' appear to be true */
    boolean already_polyd = (boolean) Upolyd;

    /* Some monsters are always of one sex and their sex can't be changed */
    /* succubi/incubi can change, but are handled below */
    /* !already_polyd check necessary because is_male() and is_female() are
       true if the player is a priest/priestess */
    if (!already_polyd ||
        (!is_male(youmonst.data) && !is_female(youmonst.data) &&
         !is_neuter(youmonst.data)))
        u.ufemale = !u.ufemale;
    if (already_polyd)  /* poly'd: also change saved sex */
        u.mfemale = !u.mfemale;
    max_rank_sz();      /* [this appears to be superfluous] */

    u.umonster = ((already_polyd ? u.mfemale : u.ufemale) &&
                  urole.femalenum != NON_PM) ? urole.femalenum : urole.malenum;
    if (!already_polyd) {
        u.umonnum = u.umonster;
    } else if (u.umonnum == PM_SUCCUBUS || u.umonnum == PM_INCUBUS) {
        u.ufemale = !u.ufemale;
        /* change monster type to match new sex */
        u.umonnum = (u.umonnum == PM_SUCCUBUS) ? PM_INCUBUS : PM_SUCCUBUS;
    }
    set_uasmon();
}

static void
newman(void)
{
    int tmp, oldlvl;

    tmp = u.uhpmax;
    oldlvl = u.ulevel;
    u.ulevel = u.ulevel - 2 + rn2_on_rng(5, rng_poly_level_adj);
    if (u.ulevel > 127 || u.ulevel < 1) {       /* level went below 0? */
        u.ulevel = oldlvl;      /* restore old level in case they lifesave */
        goto dead;
    }
    if (u.ulevel > MAXULEV)
        u.ulevel = MAXULEV;
    /* If your level goes down, your peak level goes down by the same amount so
       that you can't simply use blessed full healing to undo the decrease. But
       if your level goes up, your peak level does *not* undergo the same
       adjustment; you might end up losing out on the chance to regain some
       levels previously lost to other causes. */
    if (u.ulevel < oldlvl)
        u.ulevelmax -= (oldlvl - u.ulevel);
    if (u.ulevelmax < u.ulevel)
        u.ulevelmax = u.ulevel;

    if (!rn2(10))
        change_sex();

    adjabil(oldlvl, (int)u.ulevel);
    reset_rndmonst(NON_PM);     /* new monster generation criteria */

    /* random experience points for the new experience level */
    u.uexp = rndexp(FALSE);

    /* u.uhpmax * u.ulevel / oldlvl: proportionate hit points to new level

       -10 and +10: don't apply proportionate HP to 10 of a starting character's
       hit points (since a starting character's hit points are not on the same
       scale with hit points obtained through level gain)

       9 - rn2(19): random change of -9 to +9 hit points */
    u.uhpmax = ((u.uhpmax - 10) * (long)u.ulevel / oldlvl + 10) + (9 - rn2(19));

    u.uhp = u.uhp * (long)u.uhpmax / tmp;

    tmp = u.uenmax;
    u.uenmax = u.uenmax * (long)u.ulevel / oldlvl + 9 - rn2(19);
    if (u.uenmax < 0)
        u.uenmax = 0;
    u.uen = (tmp ? u.uen * (long)u.uenmax / tmp : u.uenmax);

    redist_attr();
    u.uhunger = rn1(500, 500);
    if (Sick)
        make_sick(0L, NULL, FALSE, SICK_ALL);
    Stoned = 0;
    set_delayed_killer(STONING, NULL);
    if (u.uhp <= 0 || u.uhpmax <= 0) {
        if (Polymorph_control) {
            if (u.uhp <= 0)
                u.uhp = 1;
            if (u.uhpmax <= 0)
                u.uhpmax = 1;
        } else {
        dead:  /* we come directly here if their experience level went to 0 or
                   less */
            pline(msgc_fatal_predone,
                  "Your new form doesn't seem healthy enough to survive.");
            done(DIED, killer_msg(DIED, "an unsuccessful polymorph"));
            newuhs(FALSE);
            return;     /* lifesaved */
        }
    }
    newuhs(FALSE);
    polyman("You feel like a new %s!",
            (u.ufemale &&
             urace.individual.f) ? urace.individual.f :
            (urace.individual.m) ? urace.individual.m : urace.noun);
    if (Slimed) {
        pline(msgc_fatal,
              "Your %s transforms, but there is still slime on you.",
              body_part(BODY));
        Slimed = 10L;
    }
    see_monsters(FALSE);
    encumber_msg();
}

void
dragonscale_polyself(void)
{
    int mntmp = armor_to_dragon(uarmc, uarm);
    int old_light = emits_light(URACEDATA);
    int new_light = FALSE;
    boolean was_floating = (Levitation || Flying);
    if (!(mvitals[mntmp].mvflags & G_GENOD)) {
        /* allow G_EXTINCT */
        pline(msgc_statusbad, "You merge with your scaly armor.");
        polymon(mntmp, TRUE);
        new_light = emits_light(URACEDATA);
        if (old_light && !new_light) {
            del_light_source(level, LS_MONSTER, &youmonst);
        } else if (new_light && !old_light) {
            new_light_source(level, u.ux, u.uy, new_light, LS_MONSTER, &youmonst);
        }
        if (is_pool(level, u.ux, u.uy) && was_floating && !(Levitation || Flying) &&
            !breathless(youmonst.data) && !amphibious(youmonst.data) && !Swimming)
            drown();
    }
}

void
polyself(boolean forcecontrol)
{
    const char *buf;
    int old_light, new_light;
    int mntmp = NON_PM;
    int tries = 0;
    boolean draconian = ((uarmc && !uskin() &&
                          ((uarmc->otyp >= GRAY_DRAGON_SCALES &&
                            uarmc->otyp <= YELLOW_DRAGON_SCALES))) ||
                         (uarm && !uskin() && uarm->scalecolor));
    boolean iswere = (u.ulycn >= LOW_PM || is_were(youmonst.data));
    boolean isvamp = (youmonst.data->mlet == S_VAMPIRE ||
                      u.umonnum == PM_VAMPIRE_BAT);
    boolean was_floating = (Levitation || Flying);

    if (!Polymorph_control && !forcecontrol && !draconian && !iswere && !isvamp) {
        int dam = 1 + rn2_on_rng(30, rng_system_shock);
        if (rn2_on_rng(20, rng_system_shock) > ACURR(A_CON)) {
            pline(msgc_nonmonbad, "You shudder for a moment.");
            losehp(dam, killer_msg(DIED, "a system shock"));
            exercise(A_CON, FALSE);
            return;
        }
    }
    old_light = emits_light(URACEDATA);

    if (Polymorph_control || forcecontrol) {
        do {
            buf = getlin("Become what kind of monster? [type the name]", FALSE);
            if (forcecontrol && !strncmp("new ", buf, 4)) {
                newman();
                goto made_change;
            }
            mntmp = name_to_mon(buf);
            if (mntmp < LOW_PM)
                pline(msgc_yafm, "I've never heard of such monsters.");
            /* Note: humans are illegal as monsters, but an illegal monster
               forces newman(), which is what we want if they specified a
               human.... */
            else if (!polyok(&mons[mntmp]) && !your_race(&mons[mntmp]))
                pline(msgc_yafm, "You cannot polymorph into that.");
            else
                break;
        } while (++tries < 5);
        if (tries == 5)
            pline(msgc_yafm, "That's enough tries!");
        /* allow skin merging, even when polymorph is controlled */
        if (draconian && (mntmp == armor_to_dragon(uarmc, uarm) || tries == 5))
            goto do_merge;
    } else if (draconian || iswere || isvamp) {
        /* special changes that don't require polyok() */
        if (draconian) {
        do_merge:
            mntmp = armor_to_dragon(uarmc, uarm);
            if (!(mvitals[mntmp].mvflags & G_GENOD)) {
                /* allow G_EXTINCT */
                pline(msgc_statusbad, "You merge with your scaly armor.");
            }
        } else if (iswere) {
            if (is_were(youmonst.data))
                mntmp = PM_HUMAN;       /* Illegal; force newman() */
            else
                mntmp = u.ulycn;
        } else {
            if (youmonst.data->mlet == S_VAMPIRE)
                mntmp = PM_VAMPIRE_BAT;
            else
                mntmp = PM_VAMPIRE;
        }
        /* if polymon fails, "you feel" message has been given so don't follow
           up with another polymon or newman */
        if (mntmp == PM_HUMAN)
            newman();   /* werecritter */
        else
            polymon(mntmp, TRUE);
        goto made_change;       /* maybe not, but this is right anyway */
    }

    if (mntmp < LOW_PM) {
        tries = 0;
        do {
            /* randomly pick an "ordinary" monster */
            mntmp = rn1(SPECIAL_PM - LOW_PM, LOW_PM);
        } while ((!polyok(&mons[mntmp]) || is_placeholder(&mons[mntmp]))
                 && tries++ < 200);
    }

    /* The below polyok() fails either if everything is genocided, or if we
       deliberately chose something illegal to force newman(). */
    if (!polyok(&mons[mntmp]) || (!forcecontrol && !rn2(5)) ||
        your_race(&mons[mntmp]))
        newman();
    else if (!polymon(mntmp, TRUE))
        return;

    if (!uarmg) {
        const char *kbuf;

        if (Upolyd) {
            kbuf = msgprintf("polymorphing into %s while wielding",
                             an(mons[u.umonnum].mname));
        } else {
            kbuf = msgprintf("returning to %s form while wielding", urace.adj);
        }
        selftouch("No longer petrify-resistant, you", kbuf);
    }

made_change:
    /* If you change this algorithm, change the matching algorithm in
       nh_create_game(). */
    new_light = emits_light(URACEDATA);
    if (old_light != new_light) {
        if (old_light)
            del_light_source(level, LS_MONSTER, &youmonst);
        if (new_light == 1)
            ++new_light;        /* otherwise it's undetectable */
        if (new_light)
            new_light_source(level, u.ux, u.uy, new_light, LS_MONSTER,
                             &youmonst);
    }
    if (is_pool(level, u.ux, u.uy) && was_floating && !(Levitation || Flying) &&
        !breathless(youmonst.data) && !amphibious(youmonst.data) && !Swimming)
        drown();
}

static int
docast_at_magc(void)
{
    return castum((struct monst *)NULL,
                  attacktype_fordmg(youmonst.data, AT_MAGC, AD_ANY));
}

static int
dogremlin_multiply(void)
{
    if (IS_FOUNTAIN(level->locations[u.ux][u.uy].typ)) {
        if (split_mon(&youmonst, NULL))
            dryup(u.ux, u.uy, TRUE);
        return 1;
    } else {
        pline(msgc_cancelled, "There's no fountain here to multiply in.");
        return 0;
    }
}

static int
dopolyself_unihorn(void)
{
    use_unicorn_horn(NULL);
    return 1;
}

static int
doshriek(void)
{
    pline(msgc_actionok, "You shriek.");
    if (u.uburied)
        pline(msgc_yafm,
              "Unfortunately sound does not carry well through rock.");
    else
        aggravate();
    return 1;
}

/* Check to see if an object in inventory provides a #monster ability.
   Currently the only things that do so are equipped red or white
   dragon scales or scale mail.

   Return TRUE if they do, FALSE if they don't. Additionally, return the
   ability itself through the pointer given as argument, if it's non-NULL.

   At present, this is only used for the player. */
static boolean
inventory_provides_polyform_ability(struct obj *ochain,
                                    struct polyform_ability *pa,
                                    struct attack *mattk)
{
    struct polyform_ability dummy;
    struct attack madummy;
    struct obj *otmp;

    if (!pa)
        pa = &dummy;
    if (!mattk)
        mattk = &madummy;

    for (otmp = ochain; otmp; otmp = otmp->nobj) {
        if ((otmp->otyp == RED_DRAGON_SCALES ||
             otmp->scalecolor == DRAGONCOLOR_RED) &&
            (otmp->owornmask & W_ARMOR)) {
            pa->description = "breathe fire";
            pa->directed = TRUE;
            pa->handler_directed = dobreathe;
            mattk->aatyp = mons[PM_RED_DRAGON].mattk[0].aatyp;
            mattk->adtyp = mons[PM_RED_DRAGON].mattk[0].adtyp;
            mattk->damn  = mons[PM_RED_DRAGON].mattk[0].damn;
            mattk->damd  = mons[PM_RED_DRAGON].mattk[0].damd;
            return TRUE;
        }
        if ((otmp->otyp == WHITE_DRAGON_SCALES ||
             otmp->scalecolor == DRAGONCOLOR_WHITE) &&
            (otmp->owornmask & W_ARMOR)) {
            pa->description = "breathe cold";
            pa->directed = TRUE;
            pa->handler_directed = dobreathe;
            mattk->aatyp = mons[PM_WHITE_DRAGON].mattk[0].aatyp;
            mattk->adtyp = mons[PM_WHITE_DRAGON].mattk[0].adtyp;
            mattk->damn  = mons[PM_WHITE_DRAGON].mattk[0].damn;
            mattk->damd  = mons[PM_WHITE_DRAGON].mattk[0].damd;
            return TRUE;
        }
    }
    return FALSE;
}

/* Check to see if the given permonst has a polyform (#monster) ability.

   Return TRUE if they do, FALSE if they don't. Additionally, return the
   ability itself through the pointer given as argument, if it's non-NULL.

   At present, this is only used for the player. It could potentially be
   expanded to other monsters in future, but that would likely need changes to
   struct polyform_ability. Given that its main purpose is for UI code, though,
   it'd make sense to keep it as player-only. */
boolean
has_polyform_ability(const struct permonst *pm,
                     struct polyform_ability *pa)
{
    struct polyform_ability dummy;
    const struct attack *atk;
    if (!pa)
        pa = &dummy;
    if ((atk = attacktype_fordmg(pm, AT_BREA, AD_ANY))) {
        const char *breathname;
        /* For now, we list only the breath types that at least one monster
           has, with an "unknown" fallthrough. Perhaps someday we'll have a
           general AT/AD rewrite. */
        switch (atk->adtyp) {
        case AD_MAGM: breathname = "breathe magic missiles"; break;
        case AD_FIRE: breathname = "breathe fire"; break;
        case AD_COLD: breathname = "breathe cold"; break;
        case AD_SLEE: breathname = "breathe sleep gas"; break;
        case AD_DISN: breathname = "breathe disintegration"; break;
        case AD_ELEC: breathname = "breathe electricity"; break;
        case AD_DRST: breathname = "breathe poison"; break;
        case AD_ACID: breathname = "breathe acid"; break;
        case AD_RBRE: breathname = "random breath weapon"; break;
        default:      breathname = "unknown breath weapon"; break;
        }
        pa->description = breathname;
        pa->directed = TRUE;
        pa->handler_directed = dobreathe;
    } else if (attacktype(pm, AT_SPIT)) {
        pa->description = "spit venom";
        pa->directed = TRUE;
        pa->handler_directed = dospit;
    } else if (attacktype(pm, AT_MAGC)) {
        pa->description = "monster magic";
        pa->directed = FALSE;
        pa->handler_undirected = docast_at_magc;
    } else if (pm->mlet == S_NYMPH) {
        pa->description = "remove iron ball";
        pa->directed = FALSE;
        pa->handler_undirected = doremove;
    } else if (attacktype(pm, AT_GAZE)) {
        pa->description = "gaze";
        pa->directed = FALSE; /* TODO: why undirected? */
        pa->handler_undirected = dogaze;
    } else if (is_were(pm)) {
        pa->description = "summon allies";
        pa->directed = FALSE;
        pa->handler_undirected = dosummon;
    } else if (webmaker(pm)) {
        pa->description = "spin web";
        pa->directed = FALSE;
        pa->handler_undirected = dospinweb;
    } else if (is_hider(pm)) {
        pa->description = "hide";
        pa->directed = FALSE;
        pa->handler_undirected = dohide;
    } else if (is_mind_flayer(pm)) {
        pa->description = "mind blast";
        pa->directed = FALSE;
        pa->handler_undirected = domindblast;
    } else if (monsndx(pm) == PM_GREMLIN) {
        pa->description = "multiply";
        pa->directed = FALSE;
        pa->handler_undirected = dogremlin_multiply;
    } else if (is_unicorn(pm)) {
        pa->description = "activate horn";
        pa->directed = FALSE;
        pa->handler_undirected = dopolyself_unihorn;
    } else if (pm->msound == MS_SHRIEK) {
        pa->description = "shriek";
        pa->directed = FALSE;
        pa->handler_undirected = doshriek;
    } else {
        return FALSE;
    }
    return TRUE;
}

/* The #monster command. Supported as a separate command for keystroke
   compatibility with older versions. Also called indirectly from the
   spellcasting code, to implement monster attacks. */
int
domonability(const struct nh_cmd_arg *arg)
{
    struct polyform_ability pa;
    struct attack dummy;
    if (has_polyform_ability(URACEDATA, &pa)) {
        if (pa.directed)
            return pa.handler_directed(arg);
        else
            return pa.handler_undirected();
    } else if (inventory_provides_polyform_ability(invent, &pa, &dummy)) {
        if (pa.directed)
            return pa.handler_directed(arg);
        else
            return pa.handler_undirected();
    } else if (Upolyd)
        pline(msgc_cancelled,
              "Any special ability you may have is purely reflexive.");
    else
        pline(msgc_cancelled,
              "You don't have a special ability in your normal form!");
    return 0;
}


/* (try to) make a mntmp monster out of the player */
/* returns 1 if polymorph successful */
int
polymon(int mntmp, boolean noisy)
{
    boolean sticky = sticks(youmonst.data) && u.ustuck &&
        !Engulfed, was_blind = ! !Blind, dochange = FALSE;
    boolean could_pass_walls = Passes_walls;
    const char *kbuf;
    int mlvl;

    if (mvitals[mntmp].mvflags & G_GENOD) {     /* allow G_EXTINCT */
        if (noisy)
            pline(msgc_noconsequence, "You feel rather %s-ish.",
                  mons[mntmp].mname);
        return 0;
    }

    if (noisy)
        break_conduct(conduct_polyself);     /* KMH, conduct */

    /* exercise used to be at the very end, but only Wis was affected
     * there since the polymorph was always in effect by then, and
     * exercising other attributes has no effect when polyselfed. */
    exercise(A_CON, FALSE);

    if (!Upolyd) {
        /* Human to monster; save human stats */
        u.macurr = u.acurr;
        u.mamax = u.amax;
        u.mfemale = u.ufemale;
    } else {
        /* Monster to monster; restore human stats, to be immediately changed
           to provide stats for the new monster */
        u.acurr = u.macurr;
        u.amax = u.mamax;
        u.ufemale = u.mfemale;
    }

    cancel_mimicking("");
    if (is_male(&mons[mntmp])) {
        if (u.ufemale)
            dochange = TRUE;
    } else if (is_female(&mons[mntmp])) {
        if (!u.ufemale)
            dochange = TRUE;
    } else if (!is_neuter(&mons[mntmp]) && mntmp != u.ulycn) {
        if (!rn2(10))
            dochange = TRUE;
    }
    if (dochange) {
        u.ufemale = !u.ufemale;
        if (noisy)
            pline(msgc_statusbad, "You %s %s%s!",
                  (u.umonnum != mntmp) ? "turn into a" : "feel like a new",
                  (is_male(&mons[mntmp]) ||
                   is_female(&mons[mntmp])) ? "" : u.ufemale ? "female " :
                  "male ", mons[mntmp].mname);
    } else if (noisy) {
        if (u.umonnum != mntmp)
            pline(msgc_statusbad, "You turn into %s!", an(mons[mntmp].mname));
        else
            pline(msgc_statusend, "You feel like a new %s!", mons[mntmp].mname);
    }
    if (Stoned && poly_when_stoned(&mons[mntmp])) {
        /* poly_when_stoned already checked stone golem genocide */
        if (noisy)
            pline(msgc_statusheal, "You turn to stone!");
        mntmp = PM_STONE_GOLEM;
        Stoned = 0;
        set_delayed_killer(STONING, NULL);
    }

    u.mtimedone = rn1(500, 500);
    u.umonnum = mntmp;
    set_uasmon();

    /* New stats for monster, to last only as long as polymorphed. Currently
       only strength gets changed. */
    if (strongmonst(&mons[mntmp]))
        ABASE(A_STR) = AMAX(A_STR) = STR18(100);

    if (Stone_resistance && Stoned) {   /* parnes@eniac.seas.upenn.edu */
        Stoned = 0;
        set_delayed_killer(STONING, NULL);
        if (noisy)
            pline(msgc_statusheal, "You no longer seem to be petrifying.");
    }
    if (Sick_resistance && Sick) {
        make_sick(0L, NULL, FALSE, SICK_ALL);
        if (noisy)
            pline(msgc_statusheal, "You no longer feel sick.");
    }
    if (Slimed) {
        if (flaming(youmonst.data)) {
            if (noisy)
                pline(msgc_statusheal, "The slime burns away!");
            Slimed = 0L;
        } else if (mntmp == PM_GREEN_SLIME || unsolid(youmonst.data)) {
            /* do it silently */
            Slimed = 0L;
        }
    }
    if (nohands(youmonst.data))
        Glib = 0;

    if (Passes_walls && u.utraptype == TT_PIT) {
        u.utraptype = 0;
        u.utrap = 0;
        turnstate.vision_full_recalc = TRUE;
    }

    /* mlvl = adj_lev(&mons[mntmp]);

       We can't do the above, since there's no such thing as an "experience
       level of you as a monster" for a polymorphed character. */
    mlvl = (int)mons[mntmp].mlevel;
    if (youmonst.data->mlet == S_DRAGON && mntmp >= PM_GRAY_DRAGON) {
        u.mhmax = In_endgame(&u.uz) ? (8 * mlvl) : (4 * mlvl + dice(mlvl, 4));
    } else if (is_golem(youmonst.data)) {
        u.mhmax = golemhp(mntmp);
    } else {
        if (!mlvl)
            u.mhmax = rnd(4);
        else
            u.mhmax = dice(mlvl, 8);
        if (is_home_elemental(&u.uz, &mons[mntmp]))
            u.mhmax *= 3;
    }
    u.mh = u.mhmax;

    if (u.ulevel < mlvl) {
        /* Low level characters can't become high level monsters for long */
        u.mtimedone = u.mtimedone * u.ulevel / mlvl;
    }

    /* At this point, if we're wearing dragon scales, umonnum and thus uskin()
       will be set correctly, so break_armor will behave correctly. */
    break_armor(noisy);
    drop_weapon(1, noisy);
    if (hides_under(youmonst.data))
        u.uundetected = OBJ_AT(u.ux, u.uy);
    else if (youmonst.data->mlet == S_KRAKEN)
        u.uundetected = is_pool(level, u.ux, u.uy);
    else
        u.uundetected = 0;

    if (u.utraptype == TT_PIT) {
        if (could_pass_walls && !Passes_walls) {
            u.utrap = rn1(6, 2);
        } else if (!could_pass_walls && Passes_walls) {
            u.utrap = 0;
        }
    }
    if (was_blind && !Blind) {  /* previous form was eyeless */
        Blinded = 1L;
        make_blinded(0L, TRUE); /* remove blindness */
    }
    newsym(u.ux, u.uy); /* Change symbol */

    if (!sticky && !Engulfed && u.ustuck && sticks(youmonst.data))
        u.ustuck = 0;
    else if (sticky && !sticks(youmonst.data))
        uunstick();
    if (u.usteed) {
        if (touch_petrifies(u.usteed->data) && !Stone_resistance && rnl(3)) {
            if (noisy)
                pline(msgc_fatal_predone,
                      "No longer petrifying-resistant, you touch %s.",
                      mon_nam(u.usteed));
            instapetrify(killer_msg(STONING,
                msgcat("riding ", an(u.usteed->data->mname))));
        }
        if (!can_ride(u.usteed))
            dismount_steed(DISMOUNT_POLY);
    }

    /* Make sure that #monster is on the supernatural abilities list. Or
       removed from the list, if we're turning into a monster that can't. */
    update_supernatural_abilities();

    if (flags.verbose && noisy) {
        struct polyform_ability pa;
        if (has_polyform_ability(youmonst.data, &pa)) {
            pline(msgc_hint, "You have a special ability in this form: '%s'.",
                  pa.description);
            pline(msgc_controlhelp, "You can cast it like a spell.");
        }

        if (lays_eggs(youmonst.data) && u.ufemale)
            pline(msgc_controlhelp,
                  "In this form, you can lay an egg by sitting on the ground.");
    }
    /* you now know what an egg of your type looks like */
    if (lays_eggs(youmonst.data)) {
        learn_egg_type(u.umonnum);
        /* make queen bees recognize killer bee eggs */
        learn_egg_type(egg_type_from_parent(u.umonnum, TRUE));
    }
    if ((!Levitation && !u.ustuck && !Flying &&
         (is_pool(level, u.ux, u.uy) || is_lava(level, u.ux, u.uy))) ||
        (Underwater && !Swimming))
        spoteffects(TRUE);
    if (Passes_walls && u.utrap && (u.utraptype == TT_INFLOOR ||
                                    u.utraptype == TT_ICEBLOCK)) {
        u.utrap = 0;
        if (noisy)
            pline(msgc_statusheal, "The %s seems to no longer trap you.",
                  u.utraptype == TT_ICEBLOCK ? "ice" : "rock");
    } else if (likes_lava(youmonst.data) && u.utrap && u.utraptype == TT_LAVA) {
        u.utrap = 0;
        if (noisy)
            pline(msgc_statusheal, "The lava now feels soothing.");
    }
    if (amorphous(youmonst.data) || is_whirly(youmonst.data) ||
        unsolid(youmonst.data)) {
        if (Punished) {
            if (noisy)
                pline(msgc_statusheal, "You slip out of the iron chain.");
            unpunish();
        }
    }
    if (u.utrap && (u.utraptype == TT_WEB || u.utraptype == TT_BEARTRAP) &&
        (amorphous(youmonst.data) || is_whirly(youmonst.data) ||
         unsolid(youmonst.data) || (youmonst.data->msize <= MZ_SMALL &&
                                    u.utraptype == TT_BEARTRAP))) {
        if (noisy)
            pline(msgc_statusheal, "You are no longer stuck in the %s.",
                  u.utraptype == TT_WEB ? "web" : "bear trap");
        /* probably should burn webs too if PM_FIRE_ELEMENTAL */
        u.utrap = 0;
    }
    if (webmaker(youmonst.data) && u.utrap && u.utraptype == TT_WEB) {
        if (noisy)
            pline(msgc_statusheal, "You orient yourself on the web.");
        u.utrap = 0;
    }
    turnstate.vision_full_recalc = TRUE;
    see_monsters(FALSE);
    if (noisy)
        encumber_msg();
    
    /* this final selftouch might trigger a recursize call to polymon()
     * [stone golem wielding cockatrice corpse and hit by stone-to-flesh,
     * becomes flesh golem above, now gets turned back into stone golem] */
    if (Upolyd) {
        kbuf = msgprintf("polymorphing into %s while wielding",
                         an(mons[u.umonnum].mname));
    } else {
        kbuf = msgprintf("returning to %s form while wielding", urace.adj);
    }
    if (!uarmg) selftouch("No longer petrify-resistant, you", kbuf);
    return 1;
}

/* Called when the player makes physical contact with bare skin with a monster
   numbered mnum, or its corpse. Normally returns FALSE. Returns TRUE if the
   player touched a *trice and wasn't saved via stone resistance or golem
   transformation, in which case the caller should print appropriate messages
   and then either call instapetrify(), or set Stoned and the delayed killer as
   appropriate.

   Also called when the player eats or is hit by an mnum egg. */
boolean
touched_monster(int mnum)
{
    return touch_petrifies(mons + mnum) && !Stone_resistance &&
        !(poly_when_stoned(youmonst.data) && polymon(PM_STONE_GOLEM, TRUE));
}

static void
break_armor(boolean noisy)
{
    struct obj *otmp;
    boolean canslitherout = slithy(youmonst.data) &&
        !humanoid(youmonst.data) && (youmonst.data->msize < MZ_HUGE);

    if ( (((otmp = uarm) != 0)) && (otmp != uskin()) &&
         (breakarm(youmonst.data, &objects[otmp->otyp]))) {
        if (canslitherout) {
            if (noisy)
                pline(msgc_statusbad, "You slither out of your armor!");
            setequip(os_arm, NULL, em_silent);
            dropx(otmp);
        } else {
            if (noisy)
                pline(msgc_itemloss, "You break out of your armor!");
            exercise(A_STR, FALSE);
            setequip(os_arm, NULL, em_silent);
            useup(otmp);
        }
    }
    if ( (((otmp = uarmc) != 0)) && (otmp != uskin()) &&
         (breakarm(youmonst.data, &objects[otmp->otyp]))) {
            if (otmp->oartifact || canslitherout) {
                if (noisy)
                    pline(msgc_statusbad, (canslitherout ?
                                           "You slither out of your %s!" :
                                           "Your %s falls off!"),
                          cloak_simple_name(otmp));
                setequip(os_armc, NULL, em_silent);
                dropx(otmp);
            } else {
                if (noisy)
                    pline(msgc_itemloss, "Your %s tears apart!",
                          cloak_simple_name(otmp));
                setequip(os_armc, NULL, em_silent);
                useup(otmp);
            }
    }
    if ( (((otmp = uarmu) != 0)) &&
         (breakarm(youmonst.data, &objects[otmp->otyp]))) {
        if (canslitherout) {
            if (noisy)
                pline(msgc_statusbad, "You slither out of your shirt!");
            setequip(os_armu, NULL, em_silent);
            dropx(otmp);
        } else {
            if (noisy)
                pline(msgc_itemloss, "Your shirt rips to shreds!");
            useup(uarmu);
        }
    }
    if ( (((otmp = uarm) != 0)) && (otmp != uskin()) &&
        /* uskin check is paranoia */
         (sliparm(youmonst.data, &objects[otmp->otyp]))) {
        if (racial_exception(&youmonst, otmp) < 1)  {
            if (noisy)
                pline(msgc_statusbad, "Your armor falls around you!");
            setequip(os_arm, NULL, em_silent);
            dropx(otmp);
        }
    }
    if ( (((otmp = uarmc) != 0)) && (otmp != uskin()) &&
         (sliparm(youmonst.data, &objects[otmp->otyp]))) {
            if (noisy) {
                if (is_whirly(youmonst.data))
                    pline(msgc_statusbad, "Your %s falls, unsupported!",
                          cloak_simple_name(otmp));
                else
                    pline(msgc_statusbad, "You shrink out of your %s!",
                          cloak_simple_name(otmp));
            }
            setequip(os_armc, NULL, em_silent);
            dropx(otmp);
    }
    if ( (((otmp = uarmu) != 0)) &&
         (sliparm(youmonst.data, &objects[otmp->otyp]))) {
            if (noisy) {
                if (is_whirly(youmonst.data))
                    pline(msgc_statusbad, "You seep right through your shirt!");
                else
                    pline(msgc_statusbad,
                          "You become much too small for your shirt!");
            }
            setequip(os_armu, NULL, em_silent);
            dropx(otmp);
    }
    if (has_horns(youmonst.data)) {
        if ((otmp = uarmh) != 0) {
            if (is_flimsy(otmp)) {
                const char *hornbuf;

                /* Future possiblities: This could damage/destroy helmet */
                hornbuf = msgcat("horn", plur(num_horns(youmonst.data)));
                if (noisy)
                    pline(msgc_consequence, "Your %s %s through %s %s.",
                          hornbuf, vtense(hornbuf, "pierce"), shk_your(otmp),
                          xname(otmp));
            } else {
                if (noisy)
                    pline(msgc_statusbad, "Your %s falls to the %s!",
                          helmet_name(otmp), surface(u.ux, u.uy));
                setequip(os_armh, NULL, em_silent);
                dropx(otmp);
            }
        }
    }
    if (nohands(youmonst.data) || verysmall(youmonst.data)) {
        if ((otmp = uarmg) != 0) {
            /* Drop weapon along with gloves */
            if (noisy)
                pline(msgc_statusbad, "You drop your gloves%s!",
                      uwep ? " and weapon" : "");
            drop_weapon(0, noisy);
            setequip(os_armg, NULL, em_silent);
            dropx(otmp);
        }
        if ((otmp = uarms) != 0) {
            if (noisy)
                pline(msgc_statusbad, "You can no longer hold your shield!");
            setequip(os_arms, NULL, em_silent);
            dropx(otmp);
        }
        if ((otmp = uarmh) != 0) {
            if (noisy)
                pline(msgc_statusbad, "Your %s falls to the %s!",
                      helmet_name(otmp), surface(u.ux, u.uy));
            setequip(os_armh, NULL, em_silent);
            dropx(otmp);
        }
    }
    if (nohands(youmonst.data) || verysmall(youmonst.data) ||
        slithy(youmonst.data) || youmonst.data->mlet == S_CENTAUR) {
        if ((otmp = uarmf) != 0) {
            if (noisy) {
                if (is_whirly(youmonst.data))
                    pline(msgc_statusbad, "Your boots fall away!");
                else
                    pline(msgc_statusbad, "Your boots %s off your feet!",
                          verysmall(youmonst.data) ? "slide" : "are pushed");
            }
            setequip(os_armf, NULL, em_silent);
            dropx(otmp);
        }
    }
}

static void
drop_weapon(int alone, boolean noisy)
{
    struct obj *otmp;
    struct obj *otmp2;
    const char *what, *which, *whichtoo;
    boolean candropwep, candropswapwep;

    if ((otmp = uwep) != 0) {
        /* !alone check below is currently superfluous but in the future it
           might not be so if there are monsters which cannot wear gloves but
           can wield weapons */
        if (!alone || cantwield(youmonst.data)) {
            struct obj *wep = uwep;
            candropwep =  canletgo(wep, "");
            candropswapwep = u.twoweap ? canletgo(uswapwep, "") : TRUE;
            /* If you're not wielding two weapons, candropswapweap
               is set to TRUE so you get the "drop" wording, since
               you are dropping your only wielded item if candropwep */

            if (alone && noisy) {
                what = (candropwep && candropswapwep) ? "drop" : "release";
                which = is_sword(uwep) ? "sword" : weapon_descr(uwep);
                if (u.twoweap) {
                    whichtoo = is_sword(uswapwep) ? "sword" :
                        weapon_descr(uswapwep);
                    if (strcmp(which, whichtoo))
                        which = "weapon";
                }
                if (uwep->quan != 1L || u.twoweap)
                    which = makeplural(which);
                
                pline(msgc_statusbad, "You find you must %s %s %s!", what,
                      the_your[!!strncmp(which, "corpse", 6)], which);
            }
            otmp2 = u.twoweap ? uswapwep : 0;
            uwepgone();
            if (candropwep)
                dropx(otmp);
            if (otmp2 != 0) {
                uswapwepgone();
                if (candropswapwep)
                    dropx(otmp2);
            }
            untwoweapon();
        } else if (!could_twoweap(youmonst.data)) {
            untwoweapon();
        }
    }
}

void
rehumanize(int how, const char *killer)
{
    if (!killer && u.mh < 1)
        impossible("Monster HP dropped to 0 without a check for death");

    /* You can't revert back while unchanging */
    if (Unchanging && u.mh < 1) {
        done(how, killer);
    }

    /* If something that would be fatal lead to rehumanization instead, there's
       less of a reason to force a --More--. So just forget about any pending
       --More-- that we might be delaying. (The alternative would be to force a
       --More-- and then reset turnstate - we have to reset it one way or the
       other - but that would likely be obnoxious, because most players don't
       consider rehumanization to be nearly as important as actual death). */
    turnstate.force_more_pending_until_done = FALSE;

    if (emits_light(youmonst.data))
        del_light_source(level, LS_MONSTER, &youmonst);
    polyman("You return to %s form!", urace.adj);

    if (u.uhp < 1) {
        /* can only happen if some bit of code reduces u.uhp
         * instead of u.mh while poly'd */
        pline(msgc_fatal, "Your old form was not healthy enough to survive.");
        done(DIED,
             killer_msg(DIED, msgcat_many("reverting to unhealthy ", urace.adj,
                                          " form", NULL)));
    }
    action_interrupted();

    turnstate.vision_full_recalc = TRUE;
    encumber_msg();
}

static int
dobreathe(const struct nh_cmd_arg *arg)
{
    struct attack ma;
    const struct attack *mattk;
    schar dx, dy, dz;

    if (Strangled) {
        pline(msgc_cancelled, "You can't breathe.  Sorry.");
        return 0;
    }
    if (u.uen < 15) {
        pline(msgc_cancelled, "You don't have enough energy to breathe!");
        return 0;
    }
    u.uen -= 15;

    if (!getargdir(arg, NULL, &dx, &dy, &dz))
        return 0;

    mattk = attacktype_fordmg(youmonst.data, AT_BREA, AD_ANY);
    if (!mattk) {
        if (inventory_provides_polyform_ability(invent, NULL, &ma)) {
            mattk = &ma;
        }
    }
    if (!mattk)
        impossible("bad breath attack?");       /* mouthwash needed... */
    else
        buzz((int)(20 + mattk->adtyp - 1), (int)mattk->damn, u.ux, u.uy, dx,
             dy, 0);
    return 1;
}

static int
dospit(const struct nh_cmd_arg *arg)
{
    struct obj *otmp;
    schar dx, dy, dz;

    if (!getargdir(arg, NULL, &dx, &dy, &dz))
        return 0;

    const struct attack *at = attacktype_fordmg(youmonst.data, AT_SPIT, AD_ANY);

    if (!at) {
        impossible ("dospit: no spitting attack");
        return 0;
    }

    switch (at->adtyp) {
    case AD_BLND:
    case AD_DRST:
        otmp = mktemp_sobj(level, BLINDING_VENOM);
        break;
    case AD_DRLI:
        otmp = mktemp_sobj(level, VAMPIRE_BLOOD);
        break;
    default:
        impossible("dospit: bad damage type");
        /* fall-through */
    case AD_ACID:
        otmp = mktemp_sobj(level, ACID_VENOM);
        break;
    }

    otmp->spe = 1;      /* to indicate it's yours */
    throwit(otmp, 0L, FALSE, dx, dy, dz, NULL);
    return 1;
}

static int
doremove(void)
{
    if (!Punished) {
        pline(msgc_cancelled, "You are not chained to anything!");
        return 0;
    }
    unpunish();
    return 1;
}

static int
dospinweb(void)
{
    struct trap *ttmp = t_at(level, u.ux, u.uy);

    if (Levitation || Is_airlevel(&u.uz) ||
        Underwater || Is_waterlevel(&u.uz)) {
        pline(msgc_cancelled, "You must be on the ground to spin a web.");
        return 0;
    }
    if (Engulfed) {
        /* TODO: Why does this take no time? (reverse msg_cancelled1) */
        pline(msgc_cancelled, "You release web fluid inside %s.",
              mon_nam(u.ustuck));
        if (is_animal(u.ustuck->data)) {
            expels(u.ustuck, u.ustuck->data, TRUE);
            return 0;
        }
        if (is_whirly(u.ustuck->data)) {
            int i;

            for (i = 0; i < NATTK; i++)
                if (u.ustuck->data->mattk[i].aatyp == AT_ENGL)
                    break;
            if (i == NATTK)
                impossible("Swallower has no engulfing attack?");
            else {
                char sweep[30];

                sweep[0] = '\0';
                switch (u.ustuck->data->mattk[i].adtyp) {
                case AD_FIRE:
                    strcpy(sweep, "ignites and ");
                    break;
                case AD_ELEC:
                    strcpy(sweep, "fries and ");
                    break;
                case AD_COLD:
                    strcpy(sweep, "freezes, shatters and ");
                    break;
                }
                pline(msgc_cancelled, "The web %sis swept away!", sweep);
            }
            return 0;
        }       /* default: a nasty jelly-like creature */
        pline(msgc_cancelled, "The web dissolves into %s.", mon_nam(u.ustuck));
        return 0;
    }
    if (u.utrap) {
        pline(msgc_cancelled, "You cannot spin webs while stuck in a trap.");
        return 0;
    }
    exercise(A_DEX, TRUE);
    if (ttmp)
        switch (ttmp->ttyp) {
        case PIT:
        case SPIKED_PIT:
            pline(msgc_actionok, "You spin a web, covering up the pit.");
            deltrap(level, ttmp);
            bury_objs(level, u.ux, u.uy);
            newsym(u.ux, u.uy);
            return 1;
        case SQKY_BOARD:
            pline(msgc_actionok, "The squeaky board is muffled.");
            deltrap(level, ttmp);
            newsym(u.ux, u.uy);
            return 1;
        case TELEP_TRAP:
        case LEVEL_TELEP:
        case MAGIC_PORTAL:
        case VIBRATING_SQUARE:
            pline(msgc_cancelled, "Your webbing vanishes!");
            return 0;
        case WEB:
            pline(msgc_yafm, "You make the web thicker.");
            return 1;
        case HOLE:
        case TRAPDOOR:
            pline(msgc_actionok, "You web over the %s.",
                  (ttmp->ttyp == TRAPDOOR) ? "trap door" : "hole");
            deltrap(level, ttmp);
            newsym(u.ux, u.uy);
            return 1;
        case ROLLING_BOULDER_TRAP:
            pline(msgc_actionok, "You spin a web, jamming the trigger.");
            deltrap(level, ttmp);
            newsym(u.ux, u.uy);
            return 1;
        case ARROW_TRAP:
        case DART_TRAP:
        case BEAR_TRAP:
        case ROCKTRAP:
        case FIRE_TRAP:
        case LANDMINE:
        case SLP_GAS_TRAP:
        case RUST_TRAP:
        case MAGIC_TRAP:
        case ANTI_MAGIC:
        case POLY_TRAP:
        case STINKING_TRAP:
            pline(msgc_actionok, "You have triggered a trap!");
            dotrap(ttmp, 0);
            return 1;
        default:
            impossible("Webbing over trap type %d?", ttmp->ttyp);
            return 0;
    } else if (On_stairs(u.ux, u.uy)) {
        /* cop out: don't let them hide the stairs */
        pline(msgc_cancelled1,
              "Your web fails to impede access to the %s.",
              (level->locations[u.ux][u.uy].typ ==
               STAIRS) ? "stairs" : "ladder");
        return 1;

    }
    ttmp = maketrap(level, u.ux, u.uy, WEB, rng_main);
    if (ttmp) {
        ttmp->madeby_u = 1;
        feeltrap(ttmp);
    }
    return 1;
}

static int
dosummon(void)
{
    int placeholder;

    if (u.uen < 10) {
        pline(msgc_cancelled,
              "You lack the energy to send forth a call for help!");
        return 0;
    }
    u.uen -= 10;

    /* TODO: This actionok is a little premature, but would need major code
       rearrangement to channelize correctly in the (rare) fail case while
       keeping message order correct */
    pline(msgc_actionok, "You call upon your brethren for help!");
    if (!were_summon(&youmonst, &placeholder, NULL))
        pline(msgc_failcurse, "But none arrive.");
    return 1;
}

static int
dogaze(void)
{
    struct monst *mtmp;
    int looked = 0;
    int i;
    uchar adtyp = 0;

    for (i = 0; i < NATTK; i++) {
        if (youmonst.data->mattk[i].aatyp == AT_GAZE) {
            adtyp = youmonst.data->mattk[i].adtyp;
            break;
        }
    }
    if (adtyp != AD_CONF && adtyp != AD_FIRE) {
        impossible("gaze attack %d?", adtyp);
        return 0;
    }

    if (Blind) {
        pline(msgc_cancelled, "You can't see anything to gaze at.");
        return 0;
    }
    if (u.uen < 15) {
        pline(msgc_cancelled, "You lack the energy to use your special gaze!");
        return 0;
    }
    u.uen -= 15;

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
        if (DEADMONSTER(mtmp))
            continue;
        if (canspotmon(mtmp) && couldsee(mtmp->mx, mtmp->my)) {
            looked++;
            if (!m_canseeu(mtmp))
                pline(msgc_failcurse, "%s seems not to notice your gaze.",
                      Monnam(mtmp));
            else if (!canseemon(mtmp))
                pline(msgc_failcurse, "You can't see where to gaze at %s.",
                      Monnam(mtmp));
            else if ((mtmp->mtame || mtmp->mpeaceful) && !Confusion)
                pline(msgc_cancelled, "You avoid gazing at %s.",
                      y_monnam(mtmp));
            else {
                /* This used to prompt for whether to attack peacefuls, or
                   attack them indiscriminately at uim_indiscriminate. Both of
                   these are a UI nightmare, really. We could consider not
                   attacking if uim_pacifist, but presumably if the player is
                   intentionally using a gaze attack, they want to do
                   /something/. */
                setmangry(mtmp);

                if (!mtmp->mcanmove || mtmp->mstun || mtmp->msleeping ||
                    !mtmp->mcansee || !haseyes(mtmp->data)) {
                    looked--;
                    continue;
                }

                /* No reflection check for consistency with when a monster
                   gazes at *you*--only medusa gaze gets reflected then. */
                if (adtyp == AD_CONF) {
                    if (!mtmp->mconf)
                        pline(msgc_combatgood, "Your gaze confuses %s!",
                              mon_nam(mtmp));
                    else
                        pline(msgc_combatgood,
                              "%s is getting more and more confused.",
                              Monnam(mtmp));
                    mtmp->mconf = 1;
                } else if (adtyp == AD_FIRE) {
                    int dmg = dice(2, 6);

                    if (resists_fire(mtmp)) {
                        pline(msgc_combatimmune,
                              "The fire doesn't burn %s!", mon_nam(mtmp));
                        dmg = 0;
                    } else
                        pline(msgc_combatgood,
                              "You attack %s with a fiery gaze!",
                              mon_nam(mtmp));
                    if ((int)u.ulevel > rn2(20))
                        destroy_mitem(mtmp, SCROLL_CLASS, AD_FIRE);
                    if ((int)u.ulevel > rn2(20))
                        destroy_mitem(mtmp, POTION_CLASS, AD_FIRE);
                    if ((int)u.ulevel > rn2(25))
                        destroy_mitem(mtmp, SPBOOK_CLASS, AD_FIRE);
                    if (dmg && !DEADMONSTER(mtmp))
                        mtmp->mhp -= dmg;
                    if (mtmp->mhp <= 0)
                        killed(mtmp);
                }
                /* For consistency with passive() in uhitm.c, this only affects
                   you if the monster is still alive. */
                if (!DEADMONSTER(mtmp) && (mtmp->data == &mons[PM_FLOATING_EYE])
                    && !mtmp->mcan) {
                    if (!Free_action) {
                        pline(msgc_statusbad, "You are frozen by %s gaze!",
                              s_suffix(mon_nam(mtmp)));
                        helpless((u.ulevel > 6 ||
                                  rn2(4)) ? dice((int)mtmp->m_lev + 1,
                                                 (int)mtmp->data->mattk[0].damd)
                                 : 200, hr_paralyzed,
                                 "frozen by gazing at a monster", NULL);
                        return 1;
                    } else
                        pline(msgc_noconsequence,
                              "You stiffen momentarily under %s gaze.",
                              s_suffix(mon_nam(mtmp)));
                }
                /* Technically this one shouldn't affect you at all because the
                   Medusa gaze is an active monster attack that only works on
                   the monster's turn, but for it to *not* have an effect would
                   be too weird. */
                if (!DEADMONSTER(mtmp) && (mtmp->data == &mons[PM_MEDUSA]) &&
                    !mtmp->mcan) {
                    pline(msgc_badidea,
                          "Gazing at the awake %s is not a very good idea.",
                          l_monnam(mtmp));
                    /* as if gazing at a sleeping anything is fruitful... */
                    pline(msgc_fatal_predone, "You turn to stone...");
                    done(STONING,
                         killer_msg(STONING,
                                    "deliberately meeting Medusa's gaze"));
                }
            }
        }
    }
    if (!looked)
        pline(msgc_notarget, "You gaze at no place in particular.");
    return 1;
}

static int
dohide(void)
{
    boolean ismimic = youmonst.data->mlet == S_MIMIC;

    if (u.uundetected || (ismimic && youmonst.m_ap_type != M_AP_NOTHING)) {
        pline(msgc_cancelled, "You are already hiding.");
        return 0;
    }
    if (ismimic) {
        /* should bring up a dialog "what would you like to imitate?" */
        youmonst.m_ap_type = M_AP_OBJECT;
        youmonst.mappearance = STRANGE_OBJECT;
    } else
        u.uundetected = 1;
    newsym(u.ux, u.uy);
    return 1;
}

static int
domindblast(void)
{
    struct monst *mtmp, *nmon;

    if (u.uen < 10) {
        pline(msgc_cancelled,
              "You concentrate but lack the energy to maintain doing so.");
        return 0;
    }
    u.uen -= 10;

    pline(msgc_occstart, "You concentrate.");
    pline_implied(msgc_occstart, "A wave of psychic energy pours out.");
    for (mtmp = level->monlist; mtmp; mtmp = nmon) {
        int u_sen;

        nmon = mtmp->nmon;
        if (DEADMONSTER(mtmp))
            continue;
        if (distu(mtmp->mx, mtmp->my) > BOLT_LIM * BOLT_LIM)
            continue;
        if (mtmp->mpeaceful)
            continue;
        u_sen = telepathic(mtmp->data) && !mtmp->mcansee;
        if (u_sen || (telepathic(mtmp->data) && rn2(2)) || !rn2(10)) {
            pline(msgc_combatgood, "You lock in on %s %s.",
                  s_suffix(mon_nam(mtmp)), u_sen ? "telepathy" :
                  telepathic(mtmp->data) ? "latent telepathy" : "mind");
            mtmp->mhp -= rnd(15);
            if (mtmp->mhp <= 0)
                killed(mtmp);
        }
    }
    return 1;
}

static void
uunstick(void)
{
    pline(msgc_statusend, "%s is no longer in your clutches.",
          Monnam(u.ustuck));
    u.ustuck = 0;
}

/* Returns whichever armor is embedded in your skin, otherwise NULL. */
struct obj *
uskin(void)
{
    if (!uarm && !uarmc)
        return NULL;
    int mntmp  = armor_to_dragon(uarmc, uarm);
    int mntmpb = armor_to_dragon(NULL, uarm);
    if ((u.umonnum == mntmp) && (mntmpb == mntmp))
        return uarm;
    else if (u.umonnum == mntmp)
        return uarmc;
    return NULL;
}

const char *
mbodypart(struct monst *mon, int part)
{
    static const char
        *const humanoid_parts[] = { "arm", "eye", "face", "finger",
        "fingertip", "foot", "hand", "handed", "head", "leg",
        "light headed", "neck", "spine", "toe", "hair",
        "blood", "lung", "nose", "stomach", "hide",
        "limbs", "skin", "body"
    }, *const jelly_parts[] = { "pseudopod", "dark spot", "front",
        "pseudopod extension", "pseudopod extremity",
        "pseudopod root", "grasp", "grasped", "cerebral area",
        "lower pseudopod", "viscous", "middle", "surface",
        "pseudopod extremity", "ripples", "juices",
        "surface", "sensor", "stomach", "surface",
        "pseudopods", "surface", "body"
    }, *const animal_parts[] =
        { "forelimb", "eye", "face", "foreclaw", "claw tip",
        "rear claw", "foreclaw", "clawed", "head", "rear limb",
        "light headed", "neck", "spine", "rear claw tip",
        "fur", "blood", "lung", "nose", "stomach", "hide",
        "limbs", "hide", "body"
    }, *const bird_parts[] = { "wing", "eye", "face", "wing", "wing tip",
        "foot", "wing", "winged", "head", "leg",
        "light headed", "neck", "spine", "toe",
        "feathers", "blood", "lung", "bill", "stomach", "plumage",
        "limbs", "skin", "body"
    }, *const gryphon_parts[] = { "foreleg", "eye", "face", "talon",
        "talon", "hind leg", "claw", "clawed", "head", "hind leg",
        "light headed", "ruff", "spine", "hind claw", "feathers",
        "blood", "lung", "beak", "stomach", "hide",
        "limbs", "hide", "body"
    }, *const horse_parts[] =
        { "foreleg", "eye", "face", "forehoof", "hoof tip",
        "rear hoof", "forehoof", "hooved", "head", "rear leg",
        "light headed", "neck", "backbone", "rear hoof tip",
        "mane", "blood", "lung", "nose", "stomach", "hide",
        "limbs", "skin", "body"
    }, *const sphere_parts[] = { "appendage", "optic nerve", "body", "tentacle",
        "tentacle tip", "lower appendage", "tentacle", "tentacled",
        "body", "lower tentacle", "rotational", "equator", "body",
        "lower tentacle tip", "cilia", "life force", "retina",
        "olfactory nerve", "interior", "cornea", "tentacles", "sclera", "body"
    }, *const fungus_parts[] = { "mycelium", "visual area", "front", "hypha",
        "hypha", "root", "strand", "stranded", "cap area",
        "rhizome", "sporulated", "stalk", "root", "rhizome tip",
        "spores", "juices", "gill", "gill", "interior", "zest",
        "hyphae", "zest", "mycelium"
    }, *const vortex_parts[] = { "region", "eye", "front", "minor current",
        "minor current", "lower current", "swirl", "swirled",
        "central core", "lower current", "addled", "center",
        "currents", "edge", "currents", "life force",
        "center", "leading edge", "interior", "wisps",
        "wisps", "outer vortex", "vortex"
    }, *const snake_parts[] = { "vestigial limb", "eye", "face", "large scale",
        "large scale tip", "rear region", "scale gap", "scale gapped",
        "head", "rear region", "light headed", "neck", "length",
        "rear scale", "scales", "blood", "lung", "forked tongue", "stomach",
        "scales", "scales", "skin", "body"
    }, *const worm_parts[] = { "anterior segment", "light sensitive cell", "clitellum",
        "setae", "setae", "posterior segment", "segment", "segmented",
        "anterior segment", "posterior", "over stretched", "clitellum", "length",
        "posterior setae", "setae", "blood", "skin", "prostomium", "stomach",
        "segments", "skin", "body"
    }, *const fish_parts[] = { "fin", "eye", "premaxillary", "pelvic axillary",
        "pelvic fin", "anal fin", "pectoral fin", "finned", "head", "peduncle",
        "played out", "gills", "dorsal fin", "caudal fin",
        "scales", "blood", "gill", "nostril", "stomach", "scales",
        "fins", "skin", "body"
    }, *const ghost_parts[] = { "arm", "eye", "face", "ghostly finger",
        "ghostly fingertip", "foot", "ghostly hand", "handed", "head",
        "leg", "light headed", "neck", "back", "toe", "aura", "spirit",
        "interior", "nose", "interior", "aura", "ghostly limbs", "aura",
        "manifestation"
    }, *const owlbear_parts[] = { "wing", "eye", "face", "wingtip",
        "wingtip", "claw", "wingtip", "winged", "head", "leg",
        "light headed", "ruff", "spine", "talon", "feathers",
        "blood", "lung", "beak", "stomach", "hide",
        "limbs", "skin", "body"
    };
    /* claw attacks are overloaded in mons[]; most humanoids with such attacks
       should still reference hands rather than claws */
    static const char not_claws[] = {
        S_HUMAN, S_QUENDI, S_MUMMY, S_ZOMBIE, S_ANGEL,
        S_NYMPH, S_HUMANOID, S_VAMPIRE,
        S_ORC, S_GIANT, /* quest nemeses */
        '\0'    /* string terminator; assert( S_xxx != 0 ); */
    };
    const struct permonst *mptr = (mon == &youmonst) ? URACEDATA : mon->data;

    if (mptr == &mons[PM_OWLBEAR]) /* These things are exceptions to several of
                                      the special cases. */
        return owlbear_parts[part];
    if (part == HAND || part == HANDED) {       /* some special cases */
        if (mptr->mlet == S_DOG || mptr->mlet == S_FELINE ||
            mptr->mlet == S_YETI || mptr == &mons[PM_SCURRIER])
            return part == HAND ? "paw" : "pawed";
        if (humanoid(mptr) && attacktype(mptr, AT_CLAW) &&
            !strchr(not_claws, mptr->mlet) && mptr != &mons[PM_STONE_GOLEM] &&
            mptr != &mons[PM_LEPRECHAUN] &&
            mptr != &mons[PM_INCUBUS] && mptr != &mons[PM_SUCCUBUS])
            return part == HAND ? "claw" : "clawed";
    }
    if ((mptr->mlet == S_ORC) && (part == NOSE))
        return "snout";
    if ((mptr == &mons[PM_MUMAK] || mptr == &mons[PM_MAMMOTH]) && part == NOSE)
        return "trunk";
    if (mptr == &mons[PM_SHARK]) {
        /* sharks don't have scales */
        if (part == HAIR)
            return "skin";
        if (part == HIDE)
            return "hide";
    }
    if (mptr == &mons[PM_JELLYFISH] || mptr == &mons[PM_KRAKEN]) {
        if ((part == ARM || part == FINGER || part == HAND ||
             part == FOOT || part == TOE))
            return "tentacle";
        if (part == LIMBS)
            return "tentacles";
    }
    if (mptr == &mons[PM_FLOATING_EYE] && part == EYE)
        return "cornea";
    if (mptr->mlet == S_WRAITH)
        return ghost_parts[part];
    if (humanoid(mptr) &&
        (part == ARM || part == FINGER || part == FINGERTIP || part == HAND ||
         part == HANDED || part == LIMBS))
        return humanoid_parts[part];
    if ((mptr)->mlet == S_JABBERWOCK && !((mptr) == &mons[PM_JABBERWOCK]))
        return gryphon_parts[part];
    if ((mptr) == &mons[PM_JABBERWOCK] && part == HAIR)
        return "scales"; /* NOT feathers */
    if ((mptr) == &mons[PM_JABBERWOCK] && part == NOSE)
        return "gaping maw"; /* NOT beak */
    if (is_bird(mptr))
        return bird_parts[part];
    if (has_beak(mptr) && part == NOSE) /* MOUTH is not a part, oddly */
        return "beak";
    if (mptr->mlet == S_CENTAUR || mptr->mlet == S_UNICORN ||
        (mptr == &mons[PM_ROTHE] && part != HAIR))
        return horse_parts[part];
    if ((mptr == &mons[PM_YELLOW_LIGHT]) ||
        (mptr == &mons[PM_BLACK_LIGHT])) {
        if (part == HANDED)
            return "rayed";
        else if (part == ARM || part == FINGER || part == FINGERTIP ||
                 part == HAND)
            return "ray";
        else if (part == LIMBS) /* lights can't be slimed... */
            return "rays";      /* but let's future-proof it anyway. */
        else if ((part == HIDE) || (part == SKIN))
            return "glow";
        else
            return "beam";
    }
    if (mptr == &mons[PM_STALKER] && part == HEAD)
        return "head";
    if (mptr->mlet == S_KRAKEN && mptr != &mons[PM_JELLYFISH])
        return fish_parts[part];
    if (mptr->mlet == S_WORM)
        return worm_parts[part];
    if (slithy(mptr) || (mptr->mlet == S_DRAGON &&
                         (part == HAIR || part == HIDE || part == SKIN)))
        return snake_parts[part];
    if (mptr->mlet == S_EYE)
        return sphere_parts[part];
    if (mptr->mlet == S_JELLY || mptr->mlet == S_PUDDING
        || mptr == &mons[PM_JELLYFISH])
        return jelly_parts[part];
    if (mptr->mlet == S_VORTEX ||
        (mptr->mlet == S_ELEMENTAL && mptr != &mons[PM_STALKER]))
        return vortex_parts[part];
    if (mptr->mlet == S_FUNGUS)
        return fungus_parts[part];
    /* Golems have substance in the right places to form
     * the body parts that are defined by shape -- hands,
     * legs, face, and so on; but they have no organs.
     * Flesh golems are the exception, of course. */
    if (mptr->mlet == S_GOLEM && !(mptr == &mons[PM_FLESH_GOLEM]) &&
        (part == HAIR || part == SPINE || part == BLOOD ||
         part == LUNG || part == STOMACH || part == EYE ||
         part == HIDE || part == SKIN)) {
        if (mptr == &mons[PM_STRAW_GOLEM] ||
            mptr == &mons[PM_PAPER_GOLEM] ||
            mptr == &mons[PM_WOOD_GOLEM] ||
            mptr == &mons[PM_ROPE_GOLEM])
            return "fiber";
        else if (mptr == &mons[PM_GOLD_GOLEM] ||
                 mptr == &mons[PM_IRON_GOLEM])
            return "metal";
        else if (mptr == &mons[PM_LEATHER_GOLEM])
            return "leather";
        else if (mptr == &mons[PM_CLAY_GOLEM] ||
                 mptr == &mons[PM_STONE_GOLEM])
            return "sediment";
        else if (mptr == &mons[PM_GLASS_GOLEM])
            return "glass";
        else
            return "substance";
    }
    if (humanoid(mptr))
        return humanoid_parts[part];
    return animal_parts[part];
}

const char *
body_part(int part)
{
    return mbodypart(&youmonst, part);
}


int
poly_gender(void)
{
/* Returns gender of polymorphed player; 0/1=same meaning as u.ufemale,
 * 2=none.
 */
    if (is_neuter(youmonst.data) || !humanoid(youmonst.data))
        return 2;
    return u.ufemale;
}


void
ugolemeffects(int damtype, int dam)
{
    int heal = 0;

    /* We won't bother with "slow"/"haste" since players do not have a
       monster-specific slow/haste so there is no way to restore the old
       velocity once they are back to human. */
    if (u.umonnum != PM_FLESH_GOLEM && u.umonnum != PM_IRON_GOLEM)
        return;
    switch (damtype) {
    case AD_ELEC:
        if (u.umonnum == PM_FLESH_GOLEM)
            heal = dam / 6;     /* Approx 1 per die */
        break;
    case AD_FIRE:
        if (u.umonnum == PM_IRON_GOLEM)
            heal = dam;
        break;
    }
    if (heal && (u.mh < u.mhmax)) {
        u.mh += heal;
        if (u.mh > u.mhmax)
            u.mh = u.mhmax;
        pline(msgc_statusheal, "Strangely, you feel better than before.");
        exercise(A_STR, TRUE);
    }
}

static int
armor_to_dragon(struct obj *scales, struct obj *mail)
{
    if (mail) {
        switch (mail->scalecolor) {
        case DRAGONCOLOR_GRAY:
            return PM_GRAY_DRAGON;
        case DRAGONCOLOR_SILVER:
            return PM_SILVER_DRAGON;
        case DRAGONCOLOR_RED:
            return PM_RED_DRAGON;
        case DRAGONCOLOR_WHITE:
            return PM_WHITE_DRAGON;
        case DRAGONCOLOR_ORANGE:
            return PM_ORANGE_DRAGON;
        case DRAGONCOLOR_BLACK:
            return PM_BLACK_DRAGON;
        case DRAGONCOLOR_BLUE:
            return PM_BLUE_DRAGON;
        case DRAGONCOLOR_GREEN:
            return PM_GREEN_DRAGON;
        case DRAGONCOLOR_YELLOW:
            return PM_YELLOW_DRAGON;
        default:
            impossible("Unknown dragon scale color: %d", mail->scalecolor);
            /* Fall Through */
        case DRAGONCOLOR_NONE:
            /* Don't return until we check the cloak slot for scales. */
            break;
        }
    }
    if (scales) {
        switch (scales->otyp) {
        case GRAY_DRAGON_SCALES:
            return PM_GRAY_DRAGON;
        case SILVER_DRAGON_SCALES:
            return PM_SILVER_DRAGON;
        case RED_DRAGON_SCALES:
            return PM_RED_DRAGON;
        case ORANGE_DRAGON_SCALES:
            return PM_ORANGE_DRAGON;
        case WHITE_DRAGON_SCALES:
            return PM_WHITE_DRAGON;
        case BLACK_DRAGON_SCALES:
            return PM_BLACK_DRAGON;
        case BLUE_DRAGON_SCALES:
            return PM_BLUE_DRAGON;
        case GREEN_DRAGON_SCALES:
            return PM_GREEN_DRAGON;
        case YELLOW_DRAGON_SCALES:
            return PM_YELLOW_DRAGON;
        default:
            return -1;
        }
    }
    return -1;
}

/*polyself.c*/
