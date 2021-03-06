/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2018-01-03 */
/* Copyright (C) 1990 by Ken Arromdee                              */
/* NetHack may be freely redistributed.  See license for details.  */

/*
 * Monster item usage routines.
 */

#include "hack.h"
#include "edog.h"

boolean m_using = FALSE;

/* Let monsters use magic items.  Arbitrary assumptions: Monsters only use
 * scrolls when they can see, monsters know when wands have 0 charges, monsters
 * cannot recognize if items are cursed are not, monsters which are confused
 * don't know not to read scrolls, etc....
 */

static const struct permonst *muse_newcham_mon(struct monst *);
static int precheck(struct monst *mon, struct obj *obj, struct musable *m);
static void mzapmsg(struct monst *, struct obj *, boolean);
static void mreadmsg(struct monst *, struct obj *);
static void mquaffmsg(struct monst *, struct obj *);
static void mbhit(struct monst *, int, struct obj *);
static void mon_break_wand(struct monst *, struct obj *);
static void mon_consume_unstone(struct monst *, struct obj *, boolean, boolean);

static int trapx, trapy;

/* Any preliminary checks which may result in the monster being unable to use
   the item.  Returns 0 if nothing happened, 2 if the monster can't do anything
   (i.e. it teleported) and 1 if it's dead. */
static int
precheck(struct monst *mon, struct obj *obj, struct musable *m)
{
    boolean vis;
    int wandlevel;

    if (!obj)
        return 0;
    vis = cansee(mon->mx, mon->my);

    if (obj->oclass == POTION_CLASS) {
        coord cc;
        static const char *const empty = "The potion turns out to be empty.";
        const char *potion_descr;
        struct monst *mtmp;

#define POTION_OCCUPANT_CHANCE(n) (13 + 2*(n))  /* also in potion.c */

        potion_descr = OBJ_DESCR(objects[obj->otyp]);
        if (potion_descr && !strcmp(potion_descr, "milky")) {
            if (flags.ghost_count < MAXMONNO &&
                !rn2(POTION_OCCUPANT_CHANCE(flags.ghost_count))) {
                if (!enexto(&cc, level, mon->mx, mon->my, &mons[PM_GHOST]))
                    return 0;
                mquaffmsg(mon, obj);
                m_useup(mon, obj);
                mtmp = makemon(&mons[PM_GHOST], level, cc.x, cc.y, NO_MM_FLAGS);
                if (!mtmp) {
                    if (vis && See_invisible)
                        pline(msgc_noconsequence, "%s", empty);
                } else {
                    if (vis && See_invisible) {
                        if (Hallucination) {
                            int idx = rndmonidx();

                            pline(msgc_monneutral,
                                  "As %s opens the bottle, %s emerges!",
                                  mon_nam(mon), monnam_is_pname(idx)
                                  ? monnam_for_index(idx)
                                  : (idx < SPECIAL_PM &&
                                     (mons[idx].geno & G_UNIQ))
                                  ? the(monnam_for_index(idx))
                                  : an(monnam_for_index(idx)));
                        } else {
                            pline(msgc_monneutral,
                                  "As %s opens the bottle, an enormous"
                                  " ghost emerges!", mon_nam(mon));
                        }
                        pline_implied(
                            combat_msgc(NULL, mon, cr_hit),
                            "%s is frightened to death, and unable to"
                            " move.", Monnam(mon));
                    }
                    mon->mcanmove = 0;
                    mon->mfrozen = 3;
                }
                return 2;
            }
        }
        /* not rng_smoky_potion; that's for wishes that players will get */
        if (potion_descr && !strcmp(potion_descr, "smoky") &&
            flags.djinni_count < MAXMONNO &&
            !rn2(POTION_OCCUPANT_CHANCE(flags.djinni_count))) {
            if (!enexto(&cc, level, mon->mx, mon->my, &mons[PM_DJINNI]))
                return 0;
            mquaffmsg(mon, obj);
            m_useup(mon, obj);
            mtmp = makemon(&mons[PM_DJINNI], level, cc.x, cc.y, NO_MM_FLAGS);
            if (!mtmp) {
                if (vis)
                    pline(msgc_noconsequence, "%s", empty);
            } else {
                if (vis)
                    pline(msgc_monneutral, "In a cloud of smoke, %s emerges!",
                          a_monnam(mtmp));
                if (canhear())
                    pline(msgc_npcvoice, "%s speaks.",
                          vis ? Monnam(mtmp) : "Something");
                /* I suspect few players will be upset that monsters */
                /* can't wish for wands of death here.... */
                if (rn2(2)) {
                    if (!Deaf)
                        verbalize(msgc_monneutral, "You freed me!");
                    msethostility(mtmp, FALSE, TRUE);
                } else {
                    if (!Deaf)
                        verbalize(msgc_monneutral, "It is about time.");
                    if (vis)
                        pline_implied(msgc_monneutral, "%s vanishes.",
                                      Monnam(mtmp));
                    mongone(mtmp);
                }
            }
            return 2;
        }
    }
    if (obj->oclass == WAND_CLASS) {
        wandlevel = getwandlevel(mon, obj);
        if (wandlevel == P_FAILURE) {
            /* critical failure */
            /* No canhear() filter: the 'if' message doesn't warrant it, 'else'
               message doesn't need it since Your_hear() has one of its own. */
            if (vis)
                pline(combat_msgc(NULL, mon, cr_hit),
                      "%s zaps %s, which suddenly explodes!", Monnam(mon),
                      an(xname(obj)));
            else
                You_hear(msgc_levelsound,
                         "a zap and an explosion in the distance.");
            mon_break_wand(mon, obj);
            m_useup(mon, obj);
            m->has_defense = m->has_offense = m->has_misc = MUSE_NONE;
            /* Only one needed to be set to MUSE_NONE but the others are harmless */
            return (DEADMONSTER(mon) ? 1 : 2);
        }
    }
    return 0;
}

static void
mzapmsg(struct monst *mtmp, struct obj *otmp, boolean self)
{
    if (!mon_visible(mtmp)) {
        int range = couldsee(mtmp->mx, mtmp->my) /* 9 or 5 in 3.6 */
            ? (BOLT_LIM + 0) : (BOLT_LIM - 3);   /* 12 or 9 for us. */
        You_hear(msgc_levelsound, "a %s zap.",
                 (distu(mtmp->mx, mtmp->my) <=
                  (range * range)) ? "nearby" : "distant");
    }
    else if (self)
        pline(combat_msgc(mtmp, NULL, cr_hit), "%s zaps %sself with %s!",
              Monnam(mtmp), mhim(mtmp), doname(otmp));
    else {
        /* TODO: channelize based on results */
        pline(combat_msgc(mtmp, NULL, cr_hit), "%s zaps %s!",
              Monnam(mtmp), an(xname(otmp)));
        action_interrupted();
    }
}

static void
mreadmsg(struct monst *mtmp, struct obj *otmp)
{
    boolean vismon = mon_visible(mtmp);
    short saverole;
    const char *onambuf;
    unsigned savebknown;

    if (!vismon && !canhear())
        return; /* no feedback */

    otmp->dknown = 1;   /* seeing or hearing it read reveals its label */
    /* shouldn't be able to hear curse/bless status of unseen scrolls; for
       priest characters, bknown will always be set during naming */
    savebknown = otmp->bknown;
    saverole = Role_switch;
    if (!vismon) {
        otmp->bknown = 0;
        if (Role_if(PM_PRIEST))
            Role_switch = 0;
    }
    onambuf = singular(otmp, doname);
    Role_switch = saverole;
    otmp->bknown = savebknown;

    if (vismon)
        pline(combat_msgc(mtmp, NULL, cr_hit), "%s reads %s!",
              Monnam(mtmp), onambuf);
    else
        You_hear(combat_msgc(mtmp, NULL, cr_hit), "%s reading %s.",
                 x_monnam(mtmp, ARTICLE_A, NULL,
                          (SUPPRESS_IT | SUPPRESS_INVISIBLE | SUPPRESS_SADDLE),
                          FALSE), onambuf);

    if (mtmp->mconf)
        pline(msgc_substitute,
              "Being confused, %s mispronounces the magic words...",
              vismon ? mon_nam(mtmp) : mhe(mtmp));
}

static void
mquaffmsg(struct monst *mtmp, struct obj *otmp)
{
    if (mon_visible(mtmp)) {
        otmp->dknown = 1;
        pline(combat_msgc(mtmp, NULL, cr_hit), "%s drinks %s!",
              Monnam(mtmp), singular(otmp, doname));
    } else
        You_hear(msgc_levelsound, "a chugging sound.");
}


/* Select a defensive item/action for a monster.  Returns TRUE iff one is
   found. */
boolean
find_defensive(struct monst *mtmp, struct musable *m)
{
    struct obj *obj = 0;
    struct trap *t;
    int x = mtmp->mx, y = mtmp->my;
    struct level *lev = mtmp->dlevel;
    boolean stuck = (mtmp == u.ustuck);
    boolean immobile = (mtmp->data->mmove == 0);
    boolean ranged_stuff = FALSE;
    int fraction;
    
    struct monst *target = mfind_target(mtmp, FALSE);

    if (target)
        ranged_stuff = TRUE;

    if (is_animal(mtmp->data) || mindless(mtmp->data))
        return FALSE;
    if (engulfing_u(mtmp) || !aware_of_u(mtmp) ||
        dist2(x, y, mtmp->mux, mtmp->muy) > 25)
        return FALSE;
    if (Engulfed && stuck)
        return FALSE;

    m->defensive = NULL;
    m->has_defense = 0;

    /* since unicorn horns don't get used up, the monster would look silly
       trying to use the same cursed horn round after round */
    if (mtmp->mconf || mtmp->mstun || !mtmp->mcansee) {
        if (!is_unicorn(mtmp->data) && !nohands(mtmp->data)) {
            for (obj = mtmp->minvent; obj; obj = obj->nobj)
                if (obj->otyp == UNICORN_HORN && !obj->cursed)
                    break;
        }
        if (obj || is_unicorn(mtmp->data)) {
            m->defensive = obj;
            m->has_defense = MUSE_UNICORN_HORN;
            return TRUE;
        }
    }

    if (mtmp->mconf) {
        for (obj = mtmp->minvent; obj; obj = obj->nobj) {
            if (obj->otyp == CORPSE && obj->corpsenm == PM_LIZARD) {
                m->defensive = obj;
                m->has_defense = MUSE_LIZARD_CORPSE;
                return TRUE;
            }
        }
    }

    /* It so happens there are two unrelated cases when we might want to check
       specifically for healing alone.  The first is when the monster is blind
       (healing cures blindness).  The second is when the monster is peaceful;
       then we don't want to flee the player, and by coincidence healing is all 
       there is that doesn't involve fleeing. These would be hard to combine
       because of the control flow. Pestilence won't use healing even when
       blind. */
    if (!mtmp->mcansee && !nohands(mtmp->data) &&
        mtmp->data != &mons[PM_PESTILENCE]) {
        if ((obj = m_carrying(mtmp, POT_FULL_HEALING)) != 0) {
            m->defensive = obj;
            m->has_defense = MUSE_POT_FULL_HEALING;
            return TRUE;
        }
        if ((obj = m_carrying(mtmp, POT_EXTRA_HEALING)) != 0) {
            m->defensive = obj;
            m->has_defense = MUSE_POT_EXTRA_HEALING;
            return TRUE;
        }
        if ((obj = m_carrying(mtmp, POT_HEALING)) != 0) {
            m->defensive = obj;
            m->has_defense = MUSE_POT_HEALING;
            return TRUE;
        }
    }

    fraction = u.ulevel < 10 ? 5 : u.ulevel < 14 ? 4 : 3;
    if (mtmp->mhp >= mtmp->mhpmax ||
        (mtmp->mhp >= 10 && mtmp->mhp * fraction >= mtmp->mhpmax))
        return FALSE;

    if (mtmp->mpeaceful) {
        if (!nohands(mtmp->data)) {
            if ((obj = m_carrying(mtmp, POT_FULL_HEALING)) != 0) {
                m->defensive = obj;
                m->has_defense = MUSE_POT_FULL_HEALING;
                return TRUE;
            }
            if ((obj = m_carrying(mtmp, POT_EXTRA_HEALING)) != 0) {
                m->defensive = obj;
                m->has_defense = MUSE_POT_EXTRA_HEALING;
                return TRUE;
            }
            if ((obj = m_carrying(mtmp, POT_HEALING)) != 0) {
                m->defensive = obj;
                m->has_defense = MUSE_POT_HEALING;
                return TRUE;
            }
        }
        return FALSE;
    }

    if (lev->locations[x][y].typ == STAIRS && !stuck && !immobile) {
        if (x == lev->dnstair.sx && y == lev->dnstair.sy &&
            !is_floater(mtmp->data))
            m->has_defense = MUSE_DOWNSTAIRS;
        if (x == lev->upstair.sx && y == lev->upstair.sy &&
            ledger_no(&u.uz) != 1)
            /* Unfair to let the monsters leave the dungeon with the Amulet
               (or go to the endlevel since you also need it, to get there) */
            m->has_defense = MUSE_UPSTAIRS;
    } else if (lev->locations[x][y].typ == LADDER && !stuck && !immobile) {
        if (x == lev->upladder.sx && y == lev->upladder.sy)
            m->has_defense = MUSE_UP_LADDER;
        if (x == lev->dnladder.sx && y == lev->dnladder.sy &&
            !is_floater(mtmp->data))
            m->has_defense = MUSE_DN_LADDER;
    } else if (lev->sstairs.sx == x && lev->sstairs.sy == y) {
        m->has_defense = MUSE_SSTAIRS;
    } else if (!stuck && !immobile) {
        /* Note: trap doors take precedence over teleport traps. */
        int xx, yy;

        for (xx = x - 1; xx <= x + 1; xx++)
            for (yy = y - 1; yy <= y + 1; yy++)
                if (isok(xx, yy))
                    if (xx != u.ux || yy != u.uy)
                        if (mtmp->data != &mons[PM_GRID_BUG] || xx == x ||
                            yy == y)
                            if ((xx == x && yy == y) || !lev->monsters[xx][yy])
                                if ((t = t_at(lev, xx, yy)) != 0)
                                    if ((verysmall(mtmp->data) ||
                                         throws_rocks(mtmp->data) ||
                                         passes_walls(mtmp->data)) ||
                                        !sobj_at(BOULDER, lev, xx, yy))
                                        if (!onscary(xx, yy, mtmp)) {
                                            if ((t->ttyp == TRAPDOOR ||
                                                 t->ttyp == HOLE)
                                                && !is_floater(mtmp->data)
                                                && !mtmp->isshk && !mtmp->isgd
                                                && !mtmp->ispriest &&
                                                can_fall_thru(lev)) {
                                                trapx = xx;
                                                trapy = yy;
                                                m->has_defense = MUSE_TRAPDOOR;
                                            } else if (t->ttyp == TELEP_TRAP &&
                                                       m->has_defense !=
                                                       MUSE_TRAPDOOR) {
                                                trapx = xx;
                                                trapy = yy;
                                                m->has_defense =
                                                    MUSE_TELEPORT_TRAP;
                                            }
                                        }
    }

    if (nohands(mtmp->data))    /* can't use objects */
        goto botm;

    if (is_mercenary(mtmp->data) && (obj = m_carrying(mtmp, BUGLE))) {
        int xx, yy;
        struct monst *mon;

        /* Distance is arbitrary.  What we really want to do is have the
           soldier play the bugle when it sees or remembers soldiers nearby...
           */
        for (xx = x - 3; xx <= x + 3; xx++)
            for (yy = y - 3; yy <= y + 3; yy++)
                if (isok(xx, yy))
                    if ((mon = m_at(lev, xx, yy)) && is_mercenary(mon->data) &&
                        mon->data != &mons[PM_GUARD] && (mon->msleeping ||
                                                         (!mon->mcanmove))) {
                        m->defensive = obj;
                        m->has_defense = MUSE_BUGLE;
                    }
    }

    /* use immediate physical escape prior to attempting magic */
    if (m->has_defense) /* stairs, trap door or tele-trap, bugle alert */
        goto botm;

    /* kludge to cut down on trap destruction (particularly portals) */
    t = t_at(lev, x, y);
    if (t &&
        (is_pit_trap(t->ttyp) || t->ttyp == WEB || t->ttyp == BEAR_TRAP))
        t = 0;  /* ok for monster to dig here */

#define nomore(x) if (m->has_defense==x) continue;
    for (obj = mtmp->minvent; obj; obj = obj->nobj) {
        /* don't always use the same selection pattern */
        if (m->has_defense && !rn2(3))
            break;

        /* nomore(MUSE_WAN_DIGGING); */
        if (m->has_defense == MUSE_WAN_DIGGING)
            break;
        if (obj->otyp == WAN_DIGGING && obj->spe > 0 && !stuck && !t &&
            !mtmp->isshk && !mtmp->isgd && !mtmp->ispriest &&
            !is_floater(mtmp->data)
            /* monsters digging in Sokoban can ruin things */
            && !In_sokoban(&u.uz)
            /* digging wouldn't be effective; assume they know that */
            && !(lev->locations[x][y].wall_info & W_NONDIGGABLE)
            && !(Is_botlevel(&u.uz) || In_endgame(&u.uz))
            && !(is_ice(lev, x, y) || is_pool(lev, x, y) || is_lava(lev, x, y))
            && !(mtmp->data == &mons[PM_VLAD_THE_IMPALER]
                 && In_V_tower(&u.uz))) {
            m->defensive = obj;
            m->has_defense = MUSE_WAN_DIGGING;
        }
        nomore(MUSE_WAN_TELEPORTATION_SELF);
        nomore(MUSE_WAN_TELEPORTATION);
        if (obj->otyp == WAN_TELEPORTATION && obj->spe > 0) {
            /* use the TELEP_TRAP bit to determine if they know about
               noteleport on this level or not.  Avoids ineffective re-use of
               teleportation.  This does mean if the monster leaves the level,
               they'll know about teleport traps. */
            if (!lev->flags.noteleport ||
                !(mtmp->mtrapseen & (1 << (TELEP_TRAP - 1)))) {
                if (mon_has_amulet(mtmp) && !ranged_stuff)
                    continue;
                m->defensive = obj;
                m->has_defense = (mon_has_amulet(mtmp))
                    ? MUSE_WAN_TELEPORTATION : MUSE_WAN_TELEPORTATION_SELF;
            }
        }
        nomore(MUSE_SCR_TELEPORTATION);
        if (obj->otyp == SCR_TELEPORTATION && mtmp->mcansee &&
            haseyes(mtmp->data)
            && (!obj->cursed || (!(mtmp->isshk && inhishop(mtmp))
                                 && !mtmp->isgd && !mtmp->ispriest))) {
            /* see WAN_TELEPORTATION case above */
            if (!lev->flags.noteleport ||
                !(mtmp->mtrapseen & (1 << (TELEP_TRAP - 1)))) {
                m->defensive = obj;
                m->has_defense = MUSE_SCR_TELEPORTATION;
            }
        }

        if (mtmp->data != &mons[PM_PESTILENCE]) {
            nomore(MUSE_POT_FULL_HEALING);
            if (obj->otyp == POT_FULL_HEALING) {
                m->defensive = obj;
                m->has_defense = MUSE_POT_FULL_HEALING;
            }
            nomore(MUSE_POT_EXTRA_HEALING);
            if (obj->otyp == POT_EXTRA_HEALING) {
                m->defensive = obj;
                m->has_defense = MUSE_POT_EXTRA_HEALING;
            }
            nomore(MUSE_WAN_CREATE_MONSTER);
            if (obj->otyp == WAN_CREATE_MONSTER && obj->spe > 0) {
                m->defensive = obj;
                m->has_defense = MUSE_WAN_CREATE_MONSTER;
            }
            nomore(MUSE_POT_HEALING);
            if (obj->otyp == POT_HEALING) {
                m->defensive = obj;
                m->has_defense = MUSE_POT_HEALING;
            }
        } else {        /* Pestilence */
            nomore(MUSE_POT_FULL_HEALING);
            if (obj->otyp == POT_SICKNESS) {
                m->defensive = obj;
                m->has_defense = MUSE_POT_FULL_HEALING;
            }
            nomore(MUSE_WAN_CREATE_MONSTER);
            if (obj->otyp == WAN_CREATE_MONSTER && obj->spe > 0) {
                m->defensive = obj;
                m->has_defense = MUSE_WAN_CREATE_MONSTER;
            }
        }
        nomore(MUSE_SCR_CREATE_MONSTER);
        if (obj->otyp == SCR_CREATE_MONSTER) {
            m->defensive = obj;
            m->has_defense = MUSE_SCR_CREATE_MONSTER;
        }
    }
botm:return (boolean) (! !m->has_defense);
#undef nomore
}

/* Perform a defensive action for a monster.  Must be called immediately
 * after find_defensive().  Return values are 0: did something, 1: died,
 * 2: did something and can't attack again (i.e. teleported).
 */
int
use_defensive(struct monst *mtmp, struct musable *m)
{
    int i, fleetim, how = 0;
    int wandlevel = 0;
    struct obj *otmp = m->defensive;
    if (otmp && otmp->oclass == WAND_CLASS)
        wandlevel = getwandlevel(mtmp, otmp);
    boolean vis, vismon, oseen;
    const char *mcsa = "%s can see again.";

    if ((i = precheck(mtmp, otmp, m)) != 0)
        return i;
    vis = cansee(mtmp->mx, mtmp->my);
    vismon = mon_visible(mtmp);
    oseen = otmp && vismon;

    /* when using defensive choice to run away, we want monster to avoid
       rushing right straight back; don't override if already scared */
    fleetim = !mtmp->mflee ? (33 - (30 * mtmp->mhp / mtmp->mhpmax)) : 0;
#define m_flee(m)       if (fleetim && !m->iswiz) \
                        { monflee(m, fleetim, FALSE, FALSE); }
    if (oseen)
        examine_object(otmp);

    switch (m->has_defense) {
    case MUSE_UNICORN_HORN:
        if (vismon) {
            /* Note if merging this with the player code: the player will need
               the "status heal" rather than "self buff" channel for this */
            if (otmp)
                pline(combat_msgc(mtmp, NULL, cr_hit),
                      "%s uses a unicorn horn!", Monnam(mtmp));
            else
                pline(combat_msgc(mtmp, NULL, cr_hit),
                      "The tip of %s's horn glows!", mon_nam(mtmp));
        }
        if (!mtmp->mcansee) {
            mtmp->mcansee = 1;
            mtmp->mblinded = 0;
            if (vismon)
                pline(combat_msgc(mtmp, NULL, cr_hit), mcsa, Monnam(mtmp));
        } else if (mtmp->mconf || mtmp->mstun) {
            mtmp->mconf = mtmp->mstun = 0;
            if (vismon)
                pline(combat_msgc(mtmp, NULL, cr_hit),
                      "%s seems steadier now.", Monnam(mtmp));
        } else
            impossible("No need for unicorn horn?");
        return 2;
    case MUSE_BUGLE:
        if (vismon)
            pline(combat_msgc(mtmp, NULL, cr_hit), "%s plays %s!",
                  Monnam(mtmp), doname(otmp));
        else
            You_hear(msgc_levelwarning, "a bugle playing reveille!");
        awaken_soldiers(mtmp);
        return 2;
    case MUSE_WAN_TELEPORTATION_SELF:
        if ((mtmp->isshk && inhishop(mtmp))
            || mtmp->isgd || mtmp->ispriest)
            return 2;
        m_flee(mtmp);
        mzapmsg(mtmp, otmp, TRUE);
        otmp->spe--;
        how = WAN_TELEPORTATION;
    mon_tele:
        if (tele_restrict(mtmp)) {      /* mysterious force... */
            if (vismon && how)  /* mentions 'teleport' */
                makeknown(how);
            /* monster learns that teleportation isn't useful here */
            if (level->flags.noteleport)
                mtmp->mtrapseen |= (1 << (TELEP_TRAP - 1));
            return 2;
        }
        if ((On_W_tower_level(&u.uz)) && !rn2(3)) {
            if (vismon)
                pline(msgc_monneutral, "%s seems disoriented for a moment.",
                      Monnam(mtmp));
            return 2;
        }
        if (oseen && how)
            makeknown(how);
        rloc(mtmp, TRUE, level);
        return 2;
    case MUSE_WAN_TELEPORTATION:
        mzapmsg(mtmp, otmp, FALSE);
        otmp->spe--;
        m_using = TRUE;
        mbhit(mtmp, rn1(8, 6), otmp);
        /* monster learns that teleportation isn't useful here */
        if (level->flags.noteleport)
            mtmp->mtrapseen |= (1 << (TELEP_TRAP - 1));
        m_using = FALSE;
        return 2;
    case MUSE_SCR_TELEPORTATION:
        {
            int obj_is_cursed = otmp->cursed;

            if (mtmp->isshk || mtmp->isgd || mtmp->ispriest)
                return 2;
            m_flee(mtmp);
            mreadmsg(mtmp, otmp);
            m_useup(mtmp, otmp);        /* otmp might be free'ed */
            how = SCR_TELEPORTATION;
            if (obj_is_cursed || mtmp->mconf) {
                int nlev;
                d_level flev;

                if (mon_has_amulet(mtmp) || In_endgame(&u.uz)) {
                    if (vismon)
                        pline(msgc_monneutral,
                              "%s seems very disoriented for a moment.",
                              Monnam(mtmp));
                    return 2;
                }
                nlev = random_teleport_level();
                if (nlev == depth(&u.uz)) {
                    if (vismon)
                        pline(msgc_monneutral, "%s shudders for a moment.",
                              Monnam(mtmp));
                    return 2;
                }
                get_level(&flev, nlev);
                migrate_to_level(mtmp, ledger_no(&flev), MIGR_RANDOM, NULL);
                if (oseen)
                    makeknown(SCR_TELEPORTATION);
            } else
                goto mon_tele;
            return 2;
        }
    case MUSE_WAN_DIGGING:
        {
            struct trap *ttmp;

            m_flee(mtmp);
            mzapmsg(mtmp, otmp, FALSE);
            otmp->spe--;
            if (oseen)
                makeknown(WAN_DIGGING);
            if (IS_FURNITURE(level->locations[mtmp->mx][mtmp->my].typ) ||
                IS_DRAWBRIDGE(level->locations[mtmp->mx][mtmp->my].typ) ||
                (drawbridge_wall_direction(mtmp->mx, mtmp->my) >= 0) ||
                (level->sstairs.sx == mtmp->mx &&
                 level->sstairs.sy == mtmp->my)) {
                pline(msgc_monneutral, "The digging ray is ineffective.");
                return 2;
            }
            if (!can_dig_down(level)) {
                if (mon_visible(mtmp))
                    pline(msgc_monneutral, "The %s here is too hard to dig in.",
                          surface(mtmp->mx, mtmp->my));
                return 2;
            }
            ttmp = maketrap(level, mtmp->mx, mtmp->my, HOLE, rng_main);
            if (!ttmp)
                return 2;
            seetrap(ttmp);
            if (vis) {
                pline(msgc_monneutral, "%s has made a hole in the %s.",
                      Monnam(mtmp), surface(mtmp->mx, mtmp->my));
                pline(msgc_monneutral, "%s %s through...", Monnam(mtmp),
                      is_flyer(mtmp->data) ? "dives" : "falls");
            } else
                You_hear(msgc_levelsound, "something crash through the %s.",
                         surface(mtmp->mx, mtmp->my));
            /* we made sure that there is a level for mtmp to go to */
            migrate_to_level(mtmp, ledger_no(&u.uz) + 1, MIGR_RANDOM, NULL);
            return 2;
        }
    case MUSE_WAN_CREATE_MONSTER:
        {
            coord cc;

            /* pm: 0 => random, eel => aquatic, croc => amphibious */
            const struct permonst *pm =
                !is_pool(level, mtmp->mx, mtmp->my) ? 0 :
                &mons[u.uinwater ? PM_GIANT_EEL : PM_CROCODILE];
            const struct permonst *fish = 0;
            int cnt = wandlevel;
            struct monst *mon;

            if (!rn2(23) || wandlevel == P_MASTER)
                cnt += rnd(7);
            if (is_pool(level, mtmp->mx, mtmp->my))
                fish = &mons[u.uinwater ? PM_GIANT_EEL : PM_CROCODILE];
            mzapmsg(mtmp, otmp, FALSE);
            otmp->spe--;
            while (cnt--) {
                /* `fish' potentially gives bias towards water locations; `pm'
                   is what to actually create (0 => random) */
                if (!enexto(&cc, level, mtmp->mx, mtmp->my, fish))
                    break;
                mon = makemon(pm, level, cc.x, cc.y,
                              MM_CREATEMONSTER | MM_CMONSTER_M);
                if (mon && cansuspectmon(mon))
                makeknown(WAN_CREATE_MONSTER);
            }
            return 2;
        }
    case MUSE_SCR_CREATE_MONSTER:
        {
            coord cc;
            const struct permonst *pm = 0, *fish = 0;
            int cnt = 1;
            struct monst *mon;
            boolean known = FALSE;

            if (!rn2(73))
                cnt += rnd(4);
            if (mtmp->mconf || otmp->cursed)
                cnt += 12;
            if (mtmp->mconf)
                pm = fish = &mons[PM_ACID_BLOB];
            else if (is_pool(level, mtmp->mx, mtmp->my))
                fish = &mons[u.uinwater ? PM_GIANT_EEL : PM_CROCODILE];
            mreadmsg(mtmp, otmp);
            while (cnt--) {
                /* `fish' potentially gives bias towards water locations; `pm'
                   is what to actually create (0 => random) */
                if (!enexto(&cc, level, mtmp->mx, mtmp->my, fish))
                    break;
                mon = makemon(pm, level, cc.x, cc.y,
                              MM_CREATEMONSTER | MM_CMONSTER_M);
                if (mon && cansuspectmon(mon))
                    known = TRUE;
            }
            /* The only case where we don't use oseen.  For wands, you have to
               be able to see the monster zap the wand to know what type it is. 
               For teleport scrolls, you have to see the monster to know it
               teleported. */
            if (known)
                makeknown(SCR_CREATE_MONSTER);
            else if (!objects[SCR_CREATE_MONSTER].oc_name_known &&
                     !objects[SCR_CREATE_MONSTER].oc_uname)
                docall(otmp);
            m_useup(mtmp, otmp);
            return 2;
        }
    case MUSE_TRAPDOOR:
        /* trap doors on "bottom" levels of dungeons are rock-drop trap doors,
           not holes in the floor.  We check here for safety. */
        if (Is_botlevel(&u.uz))
            return 0;
        m_flee(mtmp);
        if (vis) {
            struct trap *t;

            t = t_at(level, trapx, trapy);
            pline(msgc_monneutral, "%s %s into a %s!", Monnam(mtmp),
                  makeplural(locomotion(mtmp->data, "jump")),
                  t->ttyp == TRAPDOOR ? "trap door" : "hole");
            if (level->locations[trapx][trapy].typ == SCORR) {
                level->locations[trapx][trapy].typ = CORR;
                unblock_point(trapx, trapy);
            }
            seetrap(t_at(level, trapx, trapy));
        }

        /* don't use rloc_to() because worm tails must "move" */
        remove_monster(level, mtmp->mx, mtmp->my);
        newsym(mtmp->mx, mtmp->my);     /* update old location */
        place_monster(mtmp, trapx, trapy);
        if (mtmp->wormno)
            worm_move(mtmp);
        newsym(trapx, trapy);

        migrate_to_level(mtmp, ledger_no(&u.uz) + 1, MIGR_RANDOM, NULL);
        return 2;
    case MUSE_UPSTAIRS:
        /* Monsters without amulets escape the dungeon and are gone for good
           when they leave up the up stairs. Monsters with amulets would reach
           the endlevel, which we cannot allow since that would leave the
           player stranded. */
        if (ledger_no(&u.uz) == 1) {
            if (mon_has_special(mtmp))
                return 0;
            if (vismon)
                pline(msgc_monneutral, "%s escapes the dungeon!", Monnam(mtmp));
            mongone(mtmp);
            return 2;
        }
        m_flee(mtmp);
        if (vismon)
            pline(msgc_monneutral, "%s escapes upstairs!", Monnam(mtmp));
        migrate_to_level(mtmp, ledger_no(&u.uz) - 1, MIGR_STAIRS_DOWN, NULL);
        return 2;
    case MUSE_DOWNSTAIRS:
        m_flee(mtmp);
        if (vismon)
            pline(msgc_monneutral, "%s escapes downstairs!", Monnam(mtmp));
        migrate_to_level(mtmp, ledger_no(&u.uz) + 1, MIGR_STAIRS_UP, NULL);
        return 2;
    case MUSE_UP_LADDER:
        m_flee(mtmp);
        if (vismon)
            pline(msgc_monneutral, "%s escapes up the ladder!", Monnam(mtmp));
        migrate_to_level(mtmp, ledger_no(&u.uz) - 1, MIGR_LADDER_DOWN, NULL);
        return 2;
    case MUSE_DN_LADDER:
        m_flee(mtmp);
        if (vismon)
            pline(msgc_monneutral, "%s escapes down the ladder!", Monnam(mtmp));
        migrate_to_level(mtmp, ledger_no(&u.uz) + 1, MIGR_LADDER_UP, NULL);
        return 2;
    case MUSE_SSTAIRS:
        m_flee(mtmp);
        /* the stairs leading up from the 1st level are */
        /* regular stairs, not sstairs.  */
        if (level->sstairs.up) {
            if (vismon)
                pline(msgc_monneutral, "%s escapes upstairs!", Monnam(mtmp));
            if (Inhell) {
                migrate_to_level(mtmp, ledger_no(&level->sstairs.tolev),
                                 MIGR_RANDOM, NULL);
                return 2;
            }
        } else if (vismon)
            pline(msgc_monneutral, "%s escapes downstairs!", Monnam(mtmp));
        migrate_to_level(mtmp, ledger_no(&level->sstairs.tolev), MIGR_SSTAIRS,
                         NULL);
        return 2;
    case MUSE_TELEPORT_TRAP:
        m_flee(mtmp);
        if (vis) {
            pline(msgc_monneutral, "%s %s onto a teleport trap!", Monnam(mtmp),
                  makeplural(locomotion(mtmp->data, "jump")));
            if (level->locations[trapx][trapy].typ == SCORR) {
                level->locations[trapx][trapy].typ = CORR;
                unblock_point(trapx, trapy);
            }
            seetrap(t_at(level, trapx, trapy));
        }
        /* don't use rloc_to() because worm tails must "move" */
        remove_monster(level, mtmp->mx, mtmp->my);
        newsym(mtmp->mx, mtmp->my);     /* update old location */
        place_monster(mtmp, trapx, trapy);
        if (mtmp->wormno)
            worm_move(mtmp);
        newsym(trapx, trapy);

        goto mon_tele;
    case MUSE_POT_HEALING:
        mquaffmsg(mtmp, otmp);
        i = dice(6 + 2 * bcsign(otmp), 4);
        mtmp->mhp += i;
        if (mtmp->mhp > mtmp->mhpmax)
            mtmp->mhp = mtmp->mhpmax;
        if (!otmp->cursed && !mtmp->mcansee) {
            mtmp->mcansee = 1;
            mtmp->mblinded = 0;
            if (vismon)
                pline(combat_msgc(mtmp, NULL, cr_hit), mcsa, Monnam(mtmp));
        }
        if (vismon)
            pline(combat_msgc(mtmp, NULL, cr_hit), "%s looks better.",
                  Monnam(mtmp));
        if (oseen)
            makeknown(POT_HEALING);
        m_useup(mtmp, otmp);
        return 2;
    case MUSE_POT_EXTRA_HEALING:
        mquaffmsg(mtmp, otmp);
        i = dice(6 + 2 * bcsign(otmp), 8);
        mtmp->mhp += i;
        if (mtmp->mhp > mtmp->mhpmax)
            mtmp->mhp = (mtmp->mhpmax += (otmp->blessed ? 5 : 2));
        if (!mtmp->mcansee) {
            mtmp->mcansee = 1;
            mtmp->mblinded = 0;
            if (vismon)
                pline(combat_msgc(mtmp, NULL, cr_hit), mcsa, Monnam(mtmp));
        }
        if (vismon)
            pline(combat_msgc(mtmp, NULL, cr_hit), "%s looks much better.",
                  Monnam(mtmp));
        if (oseen)
            makeknown(POT_EXTRA_HEALING);
        m_useup(mtmp, otmp);
        return 2;
    case MUSE_POT_FULL_HEALING:
        mquaffmsg(mtmp, otmp);
        if (otmp->otyp == POT_SICKNESS)
            unbless(otmp);      /* Pestilence */
        mtmp->mhp = mtmp->mhpmax;
        if (!mtmp->mcansee && otmp->otyp != POT_SICKNESS) {
            mtmp->mcansee = 1;
            mtmp->mblinded = 0;
            if (vismon)
                pline(combat_msgc(mtmp, NULL, cr_hit), mcsa, Monnam(mtmp));
        }
        if (vismon)
            pline(combat_msgc(mtmp, NULL, cr_hit),
                  "%s looks completely healed.", Monnam(mtmp));
        if (oseen)
            makeknown(otmp->otyp);
        m_useup(mtmp, otmp);
        return 2;
    case MUSE_LIZARD_CORPSE:
        /* not actually called for its unstoning effect */
        mon_consume_unstone(mtmp, otmp, FALSE, FALSE);
        return 2;
    case 0:
        return 0;       /* i.e. an exploded wand */
    default:
        impossible("%s wanted to perform action %d?", Monnam(mtmp),
                   m->has_defense);
        break;
    }
    return 0;
#undef m_flee
}

int
rnd_defensive_item(struct monst *mtmp, enum rng rng)
{
    const struct permonst *pm = mtmp->data;
    int difficulty = MONSTR(monsndx(pm));
    int trycnt = 0;

    if (is_animal(pm) || attacktype(pm, AT_EXPL) || mindless(mtmp->data)
        || noncorporeal(pm) || mtmp->iskop)
        return 0;
try_again:
    switch (rn2_on_rng(8 + (difficulty > 3) + (difficulty > 6) +
                       (difficulty > 8), rng)) {
    case 6:
    case 9:
        if (mtmp->dlevel->flags.noteleport && ++trycnt < 2)
            goto try_again;
        if (!rn2_on_rng(3, rng))
            return WAN_TELEPORTATION;
        /* else FALLTHRU */
    case 0:
    case 1:
        return SCR_TELEPORTATION;
    case 8:
    case 10:
        if (!rn2_on_rng(3, rng))
            return WAN_CREATE_MONSTER;
        /* else FALLTHRU */
    case 2:
        return SCR_CREATE_MONSTER;
    case 3:
        return POT_HEALING;
    case 4:
        return POT_EXTRA_HEALING;
    case 5:
        return (mtmp->data !=
                &mons[PM_PESTILENCE]) ? POT_FULL_HEALING : POT_SICKNESS;
    case 7:
        if (is_floater(pm) || mtmp->isshk || mtmp->isgd || mtmp->ispriest)
            return 0;
        else
            return WAN_DIGGING;
    }
     /*NOTREACHED*/ return 0;
}


/* Select an offensive item/action for a monster.  Returns TRUE iff one is
   found. */
boolean
find_offensive(struct monst *mtmp, struct musable *m)
{
    struct obj *obj;
    boolean ranged_stuff = FALSE;
    boolean reflection_skip = FALSE;
    struct obj *helmet = which_armor(mtmp, os_armh);

    struct monst *target = mfind_target(mtmp, FALSE);

    if (target) {
        ranged_stuff = TRUE;
        if (target == &youmonst)
            reflection_skip = Reflecting && rn2(2);
        else
            reflection_skip = mon_reflects(target, mtmp,
                                           FALSE, NULL) && rn2(2);
        /* Skilled wand users bypass reflection. Cursed wands reduce skill,
           but monsters don't recognize BUC at the moment. */
        if (mprof(mtmp, MP_WANDS) >= MP_WAND_SKILLED)
            reflection_skip = FALSE;
    }

    m->offensive = NULL;
    m->has_offense = 0;
    if (is_animal(mtmp->data) || mindless(mtmp->data) || nohands(mtmp->data))
        return FALSE;
    if (target == &youmonst) {
        if (Engulfed)
            return FALSE;
        if (in_your_sanctuary(mtmp, 0, 0))
            return FALSE;
        if (dmgtype(mtmp->data, AD_HEAL) && !uwep && !uarmu && !uarmh
            && !uarms && !uarmg && !uarmc && !uarmf && (!uarm || uskin()))
            return FALSE;
    }

    if (!ranged_stuff)
        return FALSE; /* nothing to do */
#define nomore(x) if (m->has_offense==x) continue;
    for (obj = mtmp->minvent; obj; obj = obj->nobj) {
        if (obj->oclass == WAND_CLASS && obj->spe < 1)
            continue;
        if (!reflection_skip) {
            nomore(MUSE_WAN_DEATH);
            if (obj->otyp == WAN_DEATH) {
                m->offensive = obj;
                m->has_offense = MUSE_WAN_DEATH;
            }
            nomore(MUSE_WAN_SLEEP);
            if (obj->otyp == WAN_SLEEP &&
               ((target == &youmonst && !u_helpless(hm_paralyzed | hm_unconscious)) ||
                (target != &youmonst && target->mcanmove))) {
                m->offensive = obj;
                m->has_offense = MUSE_WAN_SLEEP;
            }
            nomore(MUSE_WAN_FIRE);
            if (obj->otyp == WAN_FIRE) {
                m->offensive = obj;
                m->has_offense = MUSE_WAN_FIRE;
            }
            nomore(MUSE_FIRE_HORN);
            if (obj->otyp == FIRE_HORN &&
                can_blow_instrument(mtmp->data)) {
                m->offensive = obj;
                m->has_offense = MUSE_FIRE_HORN;
            }
            nomore(MUSE_WAN_COLD);
            if (obj->otyp == WAN_COLD) {
                m->offensive = obj;
                m->has_offense = MUSE_WAN_COLD;
            }
            nomore(MUSE_FROST_HORN);
            if (obj->otyp == FROST_HORN &&
                can_blow_instrument(mtmp->data)) {
                m->offensive = obj;
                m->has_offense = MUSE_FROST_HORN;
            }
            nomore(MUSE_WAN_LIGHTNING);
            if (obj->otyp == WAN_LIGHTNING) {
                m->offensive = obj;
                m->has_offense = MUSE_WAN_LIGHTNING;
            }
            nomore(MUSE_WAN_MAGIC_MISSILE);
            if (obj->otyp == WAN_MAGIC_MISSILE) {
                m->offensive = obj;
                m->has_offense = MUSE_WAN_MAGIC_MISSILE;
            }
        }
        nomore(MUSE_WAN_STRIKING);
        if (obj->otyp == WAN_STRIKING) {
            m->offensive = obj;
            m->has_offense = MUSE_WAN_STRIKING;
        }
        nomore(MUSE_WAN_UNDEAD_TURNING);
        if (obj->otyp == WAN_UNDEAD_TURNING && is_undead(target->data)) {
            m->offensive = obj;
            m->has_offense = MUSE_WAN_UNDEAD_TURNING;
        }
        nomore(MUSE_WAN_SLOW_MONSTER);
        if (obj->otyp == WAN_SLOW_MONSTER &&
           ((target == &youmonst && (HFast | (TIMEOUT|INTRINSIC))) ||
            (target != &youmonst && target->mspeed != MSLOW))) {
            m->offensive = obj;
            m->has_offense = MUSE_WAN_SLOW_MONSTER;
        }
        nomore(MUSE_POT_PARALYSIS);
        if (obj->otyp == POT_PARALYSIS &&
            !u_helpless(hm_paralyzed | hm_unconscious)) {
            m->offensive = obj;
            m->has_offense = MUSE_POT_PARALYSIS;
        }
        nomore(MUSE_POT_BLINDNESS);
        if (obj->otyp == POT_BLINDNESS && !attacktype(mtmp->data, AT_GAZE)) {
            m->offensive = obj;
            m->has_offense = MUSE_POT_BLINDNESS;
        }
        nomore(MUSE_POT_CONFUSION);
        if (obj->otyp == POT_CONFUSION) {
            m->offensive = obj;
            m->has_offense = MUSE_POT_CONFUSION;
        }
        nomore(MUSE_POT_SLEEPING);
        if (obj->otyp == POT_SLEEPING) {
            m->offensive = obj;
            m->has_offense = MUSE_POT_SLEEPING;
        }
        nomore(MUSE_POT_ACID);
        if (obj->otyp == POT_ACID) {
            m->offensive = obj;
            m->has_offense = MUSE_POT_ACID;
        }
        /* we can safely put this scroll here since the locations that are in a 
           1 square radius are a subset of the locations that are in wand range 
         */
        nomore(MUSE_SCR_EARTH);
        if (obj->otyp == SCR_EARTH &&
            ((helmet && is_metallic(helmet)) || mtmp->mconf ||
             amorphous(mtmp->data) || passes_walls(mtmp->data) ||
             noncorporeal(mtmp->data) || unsolid(mtmp->data) || !rn2(10))
            && aware_of_u(mtmp) && !engulfing_u(mtmp)
            && dist2(mtmp->mx, mtmp->my, mtmp->mux, mtmp->muy) <= 2
            && mtmp->mcansee && haseyes(mtmp->data)
            && !Is_rogue_level(&u.uz)
            && (!In_endgame(&u.uz) || Is_earthlevel(&u.uz))) {
            m->offensive = obj;
            m->has_offense = MUSE_SCR_EARTH;
        }
    }
    return (boolean) (!!m->has_offense);
#undef nomore
}

/* Used for critical failures with wand use. Might also see
   use if monsters learn to break wands intelligently.
   FIXME: merge this with do_break_wand() */

/* what? (investigate do_break_wand for this...) */
#define BY_OBJECT       (NULL)

static void
mon_break_wand(struct monst *mtmp, struct obj *otmp) {
    int i, x, y;
    struct monst *mon;
    int damage;
    int expltype;
    int otyp;
    boolean oseen = mon_visible(mtmp);
    boolean affects_objects;

    otyp = otmp->otyp;
    affects_objects = FALSE;
    otmp->ox = mtmp->mx;
    otmp->oy = mtmp->my;

    /* damage */
    damage = otmp->spe * 4;
    if (otyp != WAN_MAGIC_MISSILE)
        damage *= 2;
    if (otyp == WAN_DEATH || otyp == WAN_LIGHTNING)
        damage *= 2;

    /* explosion color */
    if (otyp == WAN_FIRE)
        expltype = EXPL_FIERY;
    else if (otyp == WAN_COLD)
        expltype = EXPL_FROSTY; 
    else
        expltype = EXPL_MAGICAL;

    /* (non-sleep) ray explosions */
    if (otyp == WAN_DEATH
     || otyp == WAN_FIRE
     || otyp == WAN_COLD
     || otyp == WAN_LIGHTNING
     || otyp == WAN_MAGIC_MISSILE)
        explode(otmp->ox, otmp->oy, (otyp - WAN_MAGIC_MISSILE), damage, WAND_CLASS,
                expltype, NULL, 0);
    else {
        if (otyp == WAN_STRIKING) {
            if (oseen)
                pline(mtmp->mtame ? msgc_petcombatbad : msgc_monneutral,
                      "A wall of force smashes down around %s!",
                      mon_nam(mtmp));
            damage = dice(1 + otmp->spe, 6);
        }
        if (otyp == WAN_STRIKING
         || otyp == WAN_CANCELLATION
         || otyp == WAN_POLYMORPH
         || otyp == WAN_TELEPORTATION
         || otyp == WAN_UNDEAD_TURNING)
            affects_objects = TRUE;

        explode(otmp->ox, otmp->oy, 0, rnd(damage), WAND_CLASS, expltype, NULL, 0);

        /* affect all tiles around the monster */
        for (i = 0; i <= 8; i++) {
            bhitpos.x = x = otmp->ox + xdir[i];
            bhitpos.y = y = otmp->oy + ydir[i];
            if (!isok(x, y))
                continue;

            if (otyp == WAN_DIGGING && dig_check(BY_OBJECT, FALSE, x, y)) {
                if (IS_WALL(level->locations[x][y].typ) ||
                    IS_DOOR(level->locations[x][y].typ)) {
                    /* add potential shop damage for fixing */
                    if (*in_rooms(level, x, y, SHOPBASE))
                        add_damage(bhitpos.x, bhitpos.y, 0L);
                }
                digactualhole(x, y, BY_OBJECT,
                              (rn2(otmp->spe) < 3 ||
                               !can_dig_down(level)) ? PIT : HOLE);
            } else if (otyp == WAN_CREATE_MONSTER) {
                makemon(NULL, level, otmp->ox, otmp->oy, MM_CREATEMONSTER | MM_CMONSTER_U);
            } else {
                /* avoid telecontrol/autopickup shenanigans */
                if (x == u.ux && y == u.uy) {
                    if (otyp == WAN_TELEPORTATION &&
                        level->objects[x][y]) {
                        bhitpile(otmp, bhito, x, y);
                        bot();  /* potion effects */
                    }
                    damage = zapyourself(otmp, FALSE);
                    if (damage) {
                        losehp(damage, "killed by a wand's explosion");
                    }
                    bot();      /* blindness */
                } else if ((mon = m_at(level, x, y)) != 0) {
                    bhitm(mtmp, mon, otmp);
                }
                if (affects_objects && level->objects[x][y]) {
                    bhitpile(otmp, bhito, x, y);
                    bot();      /* potion effects */
                }
            }
        }
    }
    if (otyp == WAN_LIGHT)
    litroom(TRUE, otmp);     /* only needs to be done once */
}

/* A modified bhit() for monsters.  Based on beam_hit() in zap.c.  Unlike
 * buzz(), beam_hit() doesn't take into account the possibility of a monster
 * zapping you, so we need a special function for it.  (Unless someone wants
 * to merge the two functions...)
 */
static void
mbhit(struct monst *mon, int range, struct obj *obj) {
    struct monst *mtmp;
    struct obj *otmp;
    uchar typ;
    int ddx, ddy;

    bhitpos.x = mon->mx;
    bhitpos.y = mon->my;
    ddx = sgn(tbx);
    ddy = sgn(tby);

    while (range-- > 0) {
        int x, y;

        bhitpos.x += ddx;
        bhitpos.y += ddy;
        x = bhitpos.x;
        y = bhitpos.y;

        if (!isok(x, y)) {
            bhitpos.x -= ddx;
            bhitpos.y -= ddy;
            break;
        }
        if (find_drawbridge(&x, &y))
            switch (obj->otyp) {
            case WAN_STRIKING:
                destroy_drawbridge(x, y);
            }
        /* modified by GAN to hit all objects */
        int hitanything = 0;
        struct obj *next_obj;

        for (otmp = level->objects[bhitpos.x][bhitpos.y]; otmp;
                otmp = next_obj) {
            /* Fix for polymorph bug, Tim Wright */
            next_obj = otmp->nexthere;
            hitanything += bhito(otmp, obj);
        }
        if (hitanything)
            range--;
        if (bhitpos.x == u.ux && bhitpos.y == u.uy) {
            bhitm(mon, &youmonst, obj);
            range -= 3;
        } else if (MON_AT(level, bhitpos.x, bhitpos.y)) {
            mtmp = m_at(level, bhitpos.x, bhitpos.y);
            if (cansee(bhitpos.x, bhitpos.y) && !canspotmon(mtmp))
                map_invisible(bhitpos.x, bhitpos.y);
            bhitm(mon, mtmp, obj);
            range -= 3;
        }
        typ = level->locations[bhitpos.x][bhitpos.y].typ;
        if (IS_DOOR(typ) || typ == SDOOR) {
            switch (obj->otyp) {
                /* note: monsters don't use opening or locking magic at
                   present, but keep these as placeholders */
            case WAN_OPENING:
            case WAN_LOCKING:
            case WAN_STRIKING:
                if (doorlock(obj, bhitpos.x, bhitpos.y)) {
                    makeknown(obj->otyp);
                    /* if a shop door gets broken, add it to the shk's fix list 
                       (no cost to player) */
                    if (level->locations[bhitpos.x][bhitpos.y].doormask ==
                        D_BROKEN &&
                        *in_rooms(level, bhitpos.x, bhitpos.y, SHOPBASE))
                        add_damage(bhitpos.x, bhitpos.y, 0L);
                }
                break;
            }
        }
        if (!ZAP_POS(typ) ||
            (IS_DOOR(typ) &&
             (level->locations[bhitpos.x][bhitpos.y].
              doormask & (D_LOCKED | D_CLOSED)))
            ) {
            bhitpos.x -= ddx;
            bhitpos.y -= ddy;
            break;
        }
    }
}

/* Perform an offensive action for a monster.  Must be called immediately
   after find_offensive().  Return values are same as use_defensive(). */
int
use_offensive(struct monst *mtmp, struct musable *m)
{
    int i;
    int wandlevel = 0;
    struct obj *otmp = m->offensive;
    if (otmp && otmp->oclass == WAND_CLASS)
        wandlevel = getwandlevel(mtmp, otmp);
    boolean oseen;
    boolean isray = FALSE;

    /* offensive potions are not drunk, they're thrown */
    if (otmp->oclass != POTION_CLASS && (i = precheck(mtmp, otmp, m)) != 0)
        return i;
    oseen = mon_visible(mtmp);
    if (oseen)
        examine_object(otmp);

    /* wand efficiency is determined by a monster's proficiency
       with wands and adjusted by +/-1 based on BUC.
       cursed + unskilled results in an effect similar to breaking
       wands. */
    switch (m->has_offense) {
    case MUSE_WAN_DEATH:
    case MUSE_WAN_SLEEP:
    case MUSE_WAN_FIRE:
    case MUSE_WAN_COLD:
    case MUSE_WAN_LIGHTNING:
    case MUSE_WAN_MAGIC_MISSILE:
        isray = TRUE;
    case MUSE_WAN_TELEPORTATION:
    case MUSE_WAN_STRIKING:
    case MUSE_WAN_UNDEAD_TURNING:
    case MUSE_WAN_SLOW_MONSTER:
        mzapmsg(mtmp, otmp, FALSE);
        otmp->spe--;
        if (oseen)
            makeknown(otmp->otyp);
        m_using = TRUE;

        /* FIXME: make buzz() handle any zap */
        if (!isray) {
            mbhit(mtmp, rn1(8, 6), otmp);
            m_using = FALSE;
            return 2;
        } else {
            buzz((int)(-30 - (otmp->otyp - WAN_MAGIC_MISSILE)),
                 (wandlevel == P_UNSKILLED) ? 3 : 6, mtmp->mx, mtmp->my,
                 sgn(tbx), sgn(tby), wandlevel);
    
            m_using = FALSE;
            return DEADMONSTER(mtmp) ? 1 : 2;
        }

    case MUSE_FIRE_HORN:
    case MUSE_FROST_HORN:
        if (oseen) {
            makeknown(otmp->otyp);
            /* TODO: channelize based on results */
            pline(combat_msgc(mtmp, NULL, cr_hit), "%s plays a %s!",
                  Monnam(mtmp), xname(otmp));
        } else
            You_hear(msgc_levelsound, "a horn being played.");
        otmp->spe--;
        m_using = TRUE;
        buzz(-30 - ((otmp->otyp == FROST_HORN) ? AD_COLD - 1 : AD_FIRE - 1),
             rn1(6, 6), mtmp->mx, mtmp->my, sgn(tbx), sgn(tby), 0);
        m_using = FALSE;
        return (DEADMONSTER(mtmp)) ? 1 : 2;
    case MUSE_SCR_EARTH:
        {
            /* TODO: handle steeds */
            int x, y;

            /* don't use monster fields after killing it */
            boolean confused = (mtmp->mconf ? TRUE : FALSE);
            int mmx = mtmp->mx, mmy = mtmp->my;

            mreadmsg(mtmp, otmp);
            /* Identify the scroll */
            if (canspotmon(mtmp)) {
                pline(msgc_youdiscover, "The %s rumbles %s %s!",
                      ceiling(mtmp->mx, mtmp->my),
                      otmp->blessed ? "around" : "above", mon_nam(mtmp));
                if (oseen)
                    makeknown(otmp->otyp);
            } else if (cansee(mtmp->mx, mtmp->my)) {
                pline(msgc_youdiscover,
                      "The %s rumbles in the middle of nowhere!",
                      ceiling(mtmp->mx, mtmp->my));
                map_invisible(mtmp->mx, mtmp->my);
                if (oseen)
                    makeknown(otmp->otyp);
            }

            /* TODO: Merge the monster and player parts of this code (and
               ideally get it to be a little less heavily nested) */
            
            /* Loop through the surrounding squares */
            for (x = mmx - 1; x <= mmx + 1; x++) {
                for (y = mmy - 1; y <= mmy + 1; y++) {
                    /* Is this a suitable spot? */
                    if (isok(x, y) && !closed_door(level, x, y) &&
                        !IS_ROCK(level->locations[x][y].typ) &&
                        !IS_AIR(level->locations[x][y].typ) &&
                        (((x == mmx) &&
                          (y == mmy)) ? !otmp->blessed : !otmp->cursed) &&
                        (x != u.ux || y != u.uy)) {
                        struct obj *otmp2;
                        struct monst *mtmp2;

                        /* Make the object(s) */
                        otmp2 = mksobj(level, confused ? ROCK : BOULDER,
                                       FALSE, FALSE, rng_main);
                        if (!otmp2)
                            continue;   /* Shouldn't happen */
                        otmp2->quan = confused ? rn1(5, 2) : 1;
                        otmp2->owt = weight(otmp2);

                        /* Find the monster here (might be same as mtmp) */
                        mtmp2 = m_at(level, x, y);
                        if (mtmp2 && !amorphous(mtmp2->data) &&
                            !passes_walls(mtmp2->data) &&
                            !noncorporeal(mtmp2->data) &&
                            !unsolid(mtmp2->data)) {
                            struct obj *helmet = which_armor(mtmp2, os_armh);
                            int mdmg;

                            if (cansee(mtmp2->mx, mtmp2->my)) {
                                if (!helmet || !is_metallic(helmet))
                                    pline(combat_msgc(mtmp, mtmp2, cr_hit),
                                          "%s is hit by %s!", Monnam(mtmp2),
                                          doname(otmp2));
                                if (!canspotmon(mtmp2))
                                    map_invisible(mtmp2->mx, mtmp2->my);
                            }
                            mdmg = dmgval(otmp2, mtmp2) * otmp2->quan;
                            if (helmet) {
                                if (is_metallic(helmet)) {
                                    if (canseemon(mtmp2))
                                        pline(combat_msgc(mtmp, mtmp2,
                                                          cr_resist),
                                              "A %s bounces off %s hard %s.",
                                              doname(otmp2),
                                              s_suffix(mon_nam(mtmp2)),
                                              helmet_name(helmet));
                                    else
                                        You_hear(msgc_levelsound,
                                                 "a clanging sound.");
                                    if (mdmg > 2)
                                        mdmg = 2;
                                } else {
                                    if (canseemon(mtmp2))
                                        pline_implied(
                                            combat_msgc(mtmp, mtmp2, cr_hit),
                                            "%s's %s does not protect %s.",
                                            Monnam(mtmp2), xname(helmet),
                                            mhim(mtmp2));
                                }
                            }
                            mtmp2->mhp -= mdmg;
                            if (mtmp2->mhp <= 0) {
                                pline(combat_msgc(mtmp, mtmp2, cr_kill),
                                      "%s is killed.", Monnam(mtmp2));
                                mondied(mtmp2);
                            }
                        }
                        /* Drop the rock/boulder to the floor */
                        if (!flooreffects(otmp2, x, y, "fall")) {
                            place_object(otmp2, level, x, y);
                            stackobj(otmp2);
                            newsym(x, y);       /* map the rock */
                        }
                    }
                }
            }
            m_useup(mtmp, otmp);
            /* Attack the player */
            if (distmin(mmx, mmy, u.ux, u.uy) == 1 && !otmp->cursed) {
                int dmg;
                struct obj *otmp2;

                /* Okay, _you_ write this without repeating the code */
                otmp2 = mksobj(level, confused ? ROCK : BOULDER, FALSE, FALSE,
                               rng_main);
                if (!otmp2)
                    goto xxx_noobj;     /* Shouldn't happen */
                otmp2->quan = confused ? rn1(5, 2) : 1;
                otmp2->owt = weight(otmp2);
                if (Hallucination) {
                    pline(msgc_playerimmune, "You are already stoned.");
                    dmg = 0;
                } else if (!amorphous(youmonst.data) && !Passes_walls &&
                    !noncorporeal(youmonst.data) && !unsolid(youmonst.data)) {
                    pline(combat_msgc(mtmp, &youmonst, cr_hit),
                          "You are hit by %s!", doname(otmp2));
                    dmg = dmgval(otmp2, &youmonst) * otmp2->quan;
                    if (uarmh) {
                        if (is_metallic(uarmh)) {
                            pline(msgc_playerimmune,
                                  "Fortunately, you are wearing a hard %s.",
                                  helmet_name(uarmh));
                            if (dmg > 2)
                                dmg = 2;
                        } else {
                            pline_implied(msgc_notresisted,
                                          "Your %s does not protect you.",
                                          xname(uarmh));
                        }
                    }
                } else
                    dmg = 0;
                if (!flooreffects(otmp2, u.ux, u.uy, "fall")) {
                    place_object(otmp2, level, u.ux, u.uy);
                    stackobj(otmp2);
                    newsym(u.ux, u.uy);
                }
                if (dmg)
                    losehp(dmg, killer_msg(DIED, "scroll of earth"));
            }
        xxx_noobj:

            return (DEADMONSTER(mtmp)) ? 1 : 2;
        }
    case MUSE_POT_PARALYSIS:
    case MUSE_POT_BLINDNESS:
    case MUSE_POT_CONFUSION:
    case MUSE_POT_SLEEPING:
    case MUSE_POT_ACID:
        /* Note: this setting of dknown doesn't suffice.  A monster which is
           out of sight might throw and it hits something _in_ sight, a problem 
           not existing with wands because wand rays are not objects.  Also set 
           dknown in mthrowu.c. */
        if (cansee(mtmp->mx, mtmp->my)) {
            otmp->dknown = 1;
            /* TODO: channelize based on results */
            pline(combat_msgc(mtmp, NULL, cr_hit), "%s hurls %s!",
                  Monnam(mtmp), singular(otmp, doname));
        }
        /* Wow, this is a twisty mess of ugly global variables. */
        if (engulfing_u(mtmp))
            panic("Monster throws potion while engulfing you!");
        m_throw(mtmp, mtmp->mx, mtmp->my, sgn(tbx), sgn(tby),
                distmin(0, 0, tbx, tby), otmp, TRUE);
        return 2;
    case 0:
        return 0;       /* i.e. an exploded wand */
    default:
        impossible("%s wanted to perform action %d?", Monnam(mtmp),
                   m->has_offense);
        break;
    }
    return 0;
}

int
rnd_offensive_item(struct monst *mtmp, enum rng rng)
{
    const struct permonst *pm = mtmp->data;
    int difficulty = MONSTR(monsndx(pm));

    if (is_animal(pm) || attacktype(pm, AT_EXPL) || mindless(mtmp->data)
        || noncorporeal(pm) || mtmp->iskop)
        return 0;
    if (difficulty > 7 && !rn2_on_rng(35, rng))
        return WAN_DEATH;
    switch (rn2_on_rng(9 - (difficulty < 4) + 4 * (difficulty > 6), rng)) {
    case 0:{
            struct obj *helmet = which_armor(mtmp, os_armh);

            if ((helmet && is_metallic(helmet)) || amorphous(pm) ||
                passes_walls(pm) || noncorporeal(pm) || unsolid(pm))
                return SCR_EARTH;
        }       /* fall through */
    case 1:
        return WAN_STRIKING;
    case 2:
        return POT_ACID;
    case 3:
        return POT_CONFUSION;
    case 4:
        return POT_BLINDNESS;
    case 5:
        return POT_SLEEPING;
    case 6:
        return POT_PARALYSIS;
    case 7:
    case 8:
        return WAN_MAGIC_MISSILE;
    case 9:
        return WAN_SLEEP;
    case 10:
        return WAN_FIRE;
    case 11:
        return WAN_COLD;
    case 12:
        return WAN_LIGHTNING;
    }
     /*NOTREACHED*/ return 0;
}

boolean
find_misc(struct monst * mtmp, struct musable * m)
{
    struct obj *obj;
    const struct permonst *mdat = mtmp->data;
    int x = mtmp->mx, y = mtmp->my;
    struct trap *t;
    int xx, yy;
    boolean immobile = (mdat->mmove == 0);
    boolean stuck = (mtmp == u.ustuck);
    boolean ranged_stuff = FALSE;

    struct monst *target = mfind_target(mtmp, TRUE); /* zaps here is helpful */
    const struct permonst *tdat = NULL;
    if (target) {
        tdat = target->data;
        ranged_stuff = TRUE;
    }

    m->misc = NULL;
    m->has_misc = 0;
    if (is_animal(mdat) || mindless(mdat))
        return 0;
    if (Engulfed && stuck)
        return FALSE;

    /* We arbitrarily limit to times when a player is nearby for the same
       reason as Junior Pac-Man doesn't have energizers eaten until you can see 
       them... */
    if (!aware_of_u(mtmp) || engulfing_u(mtmp) ||
        dist2(x, y, mtmp->mux, mtmp->muy) > 36)
        return FALSE;

    if (!stuck && !immobile && !mtmp->cham && MONSTR(monsndx(mdat)) < 6) {
        boolean ignore_boulders = (verysmall(mdat) || throws_rocks(mdat) ||
                                   passes_walls(mdat));
        for (xx = x - 1; xx <= x + 1; xx++)
            for (yy = y - 1; yy <= y + 1; yy++)
                if (isok(xx, yy) && (xx != u.ux || yy != u.uy))
                    if (mdat != &mons[PM_GRID_BUG] || xx == x || yy == y)
                        if ( /* (xx==x && yy==y) || */ !level->monsters[xx][yy])
                            if ((t = t_at(level, xx, yy)) != 0 &&
                                (ignore_boulders ||
                                 !sobj_at(BOULDER, level, xx, yy))
                                && !onscary(xx, yy, mtmp)) {
                                if (t->ttyp == POLY_TRAP) {
                                    trapx = xx;
                                    trapy = yy;
                                    m->has_misc = MUSE_POLY_TRAP;
                                    return TRUE;
                                }
                            }
    }
    if (nohands(mdat))
        return 0;

#define nomore(x) if (m->has_misc==x) continue;
    for (obj = mtmp->minvent; obj; obj = obj->nobj) {
        if (obj && obj->oclass == WAND_CLASS && obj->spe < 1)
            continue;
        /* Monsters shouldn't recognize cursed items; this kludge is */
        /* necessary to prevent serious problems though... */
        if (obj->otyp == POT_GAIN_LEVEL &&
            (!obj->cursed ||
             (!mtmp->isgd && !mtmp->isshk && !mtmp->ispriest))) {
            m->misc = obj;
            m->has_misc = MUSE_POT_GAIN_LEVEL;
        }
        nomore(MUSE_BULLWHIP);
        if (obj->otyp == BULLWHIP && (MON_WEP(mtmp) == obj) &&
            distu(mtmp->mx, mtmp->my) == 1 && uwep && !mtmp->mpeaceful) {
            m->misc = obj;
            m->has_misc = MUSE_BULLWHIP;
        }
        /* Note: peaceful/tame monsters won't make themselves invisible unless
           you can see them.  Not really right, but... */
        nomore(MUSE_WAN_MAKE_INVISIBLE_SELF);
        if (obj->otyp == WAN_MAKE_INVISIBLE && !mtmp->minvis &&
            !mtmp->invis_blkd && (!mtmp->mpeaceful || See_invisible) &&
            (!attacktype(mtmp->data, AT_GAZE) || mtmp->mcan)) {
            m->misc = obj;
            m->has_misc = MUSE_WAN_MAKE_INVISIBLE_SELF;
            continue;
        }
        nomore(MUSE_POT_INVISIBILITY);
        if (obj->otyp == POT_INVISIBILITY && !mtmp->minvis && !mtmp->invis_blkd
            && (!mtmp->mpeaceful || See_invisible) &&
            (!attacktype(mtmp->data, AT_GAZE) || mtmp->mcan)) {
            m->misc = obj;
            m->has_misc = MUSE_POT_INVISIBILITY;
        }
        nomore(MUSE_WAN_MAKE_INVISIBLE);
        if (ranged_stuff && target != &youmonst &&
            obj->otyp == WAN_MAKE_INVISIBLE &&
            !target->minvis && !mtmp->invis_blkd &&
            (!mtmp->mpeaceful || See_invisible) &&
            (!attacktype(target->data, AT_GAZE) || target->mcan)) {
            m->misc = obj;
            m->has_misc = MUSE_WAN_MAKE_INVISIBLE;
        }
        nomore(MUSE_WAN_SPEED_MONSTER_SELF);
        if (obj->otyp == WAN_SPEED_MONSTER &&
            mtmp->mspeed != MFAST && !mtmp->isgd) {
            m->misc = obj;
            m->has_misc = MUSE_WAN_SPEED_MONSTER_SELF;
            continue;
        }
        nomore(MUSE_POT_SPEED);
        if (obj->otyp == POT_SPEED && mtmp->mspeed != MFAST && !mtmp->isgd) {
            m->misc = obj;
            m->has_misc = MUSE_POT_SPEED;
        }
        nomore(MUSE_WAN_SPEED_MONSTER);
        if (ranged_stuff && obj->otyp == WAN_SPEED_MONSTER &&
            ((target != &youmonst && target->mspeed != MFAST &&
            !target->isgd) || (target == &youmonst && !(HFast & INTRINSIC)))) {
            m->misc = obj;
            m->has_misc = MUSE_WAN_SPEED_MONSTER;
        }
        nomore(MUSE_WAN_POLYMORPH_SELF);
        if (obj->otyp == WAN_POLYMORPH && !mtmp->cham &&
            MONSTR(monsndx(mdat)) < 6) {
            m->misc = obj;
            m->has_misc = MUSE_WAN_POLYMORPH_SELF;
            continue;
        }
        nomore(MUSE_POT_POLYMORPH);
        if (obj->otyp == POT_POLYMORPH && !mtmp->cham &&
            MONSTR(monsndx(mdat)) < 6) {
            m->misc = obj;
            m->has_misc = MUSE_POT_POLYMORPH;
        }
        nomore(MUSE_WAN_POLYMORPH);
        if (ranged_stuff && target != &youmonst &&
            obj->otyp == WAN_POLYMORPH && !target->cham && !resists_magm(target) &&
            (MONSTR(monsndx(tdat)) < 6 || mprof(mtmp, MP_WANDS) == MP_WAND_EXPERT)) {
            m->misc = obj;
            m->has_misc = MUSE_WAN_POLYMORPH;
        }
        nomore(MUSE_SCR_REMOVE_CURSE);
        if (obj->otyp == SCR_REMOVE_CURSE) {
            struct obj *otmp;

            for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj) {
                if (otmp->cursed &&
                    (otmp->otyp == LOADSTONE || otmp->owornmask)) {
                    m->misc = obj;
                    m->has_misc = MUSE_SCR_REMOVE_CURSE;
                }
            }
        }
    }
    return (boolean) (! !m->has_misc);
#undef nomore
}

/* type of monster to polymorph into; defaults to one suitable for the
   current level rather than the totally arbitrary choice of newcham() */
static const struct permonst *
muse_newcham_mon(struct monst *mon)
{
    struct obj *m_armr;

    if ((m_armr = which_armor(mon, os_arm)) != 0) {
        if (Is_dragon_scales(m_armr))
            return Dragon_scales_to_pm(m_armr);
        else if (Is_dragon_mail(m_armr))
            return Dragon_mail_to_pm(m_armr);
    }
    return rndmonst(&mon->dlevel->z, rng_main);
}

int
use_misc(struct monst *mtmp, struct musable *m)
{
    int i;
    struct obj *otmp = m->misc;
    boolean vismon, oseen;
    const char *nambuf;

    enum msg_channel msgc = mtmp->mtame ? msgc_petneutral : msgc_monneutral;
    
    if ((i = precheck(mtmp, otmp, m)) != 0)
        return i;
    vismon = mon_visible(mtmp);
    oseen = otmp && vismon;

    switch (m->has_misc) {
    case MUSE_WAN_MAKE_INVISIBLE:
    case MUSE_WAN_SPEED_MONSTER:
    case MUSE_WAN_POLYMORPH:
        mzapmsg(mtmp, otmp, FALSE);
        otmp->spe--;
        m_using = TRUE;
        mbhit(mtmp, rn1(8, 6), otmp);
        m_using = FALSE;
        return 2;
    case MUSE_WAN_MAKE_INVISIBLE_SELF:
    case MUSE_WAN_SPEED_MONSTER_SELF:
    case MUSE_WAN_POLYMORPH_SELF:
        mzapmsg(mtmp, otmp, TRUE);
        otmp->spe--;
        m_using = TRUE;
        bhitm(mtmp, mtmp, otmp);
        m_using = FALSE;
        return 2;
    case MUSE_POT_GAIN_LEVEL:
        mquaffmsg(mtmp, otmp);
        if (otmp->cursed) {
            if (Can_rise_up(mtmp->mx, mtmp->my, &u.uz)) {
                int tolev = depth(&u.uz) - 1;
                d_level tolevel;

                get_level(&tolevel, tolev);
                /* insurance against future changes... */
                if (on_level(&tolevel, &u.uz))
                    goto skipmsg;
                if (vismon) {
                    pline(msgc, "%s rises up, through the %s!",
                          Monnam(mtmp), ceiling(mtmp->mx, mtmp->my));
                    if (!objects[POT_GAIN_LEVEL].oc_name_known &&
                        !objects[POT_GAIN_LEVEL].oc_uname)
                        docall(otmp);
                }
                m_useup(mtmp, otmp);
                migrate_to_level(mtmp, ledger_no(&tolevel), MIGR_RANDOM, NULL);
                return 2;
            } else {
            skipmsg:
                if (vismon) {
                    pline(msgc, "%s looks uneasy.", Monnam(mtmp));
                    if (!objects[POT_GAIN_LEVEL].oc_name_known &&
                        !objects[POT_GAIN_LEVEL].oc_uname)
                        docall(otmp);
                }
                m_useup(mtmp, otmp);
                return 2;
            }
        }
        if (vismon)
            pline(msgc, "%s seems more experienced.", Monnam(mtmp));
        if (oseen)
            makeknown(POT_GAIN_LEVEL);
        m_useup(mtmp, otmp);
        if (!grow_up(mtmp, NULL))
            return 1;
        /* grew into genocided monster */
        return 2;
    case MUSE_POT_INVISIBILITY:
        if (otmp->otyp == WAN_MAKE_INVISIBLE) {
            mzapmsg(mtmp, otmp, TRUE);
            otmp->spe--;
        } else
            mquaffmsg(mtmp, otmp);
        /* format monster's name before altering its visibility */
        nambuf = (See_invisible || tp_sensemon(mtmp)) ?
            Monnam(mtmp) : mon_nam(mtmp);
        mon_set_minvis(mtmp);
        if (vismon && mtmp->minvis) {   /* was seen, now invisible */
            if (See_invisible)
                pline(msgc, "%s %s takes on a %s transparency.",
                      s_suffix(nambuf), mbodypart(mtmp, BODY),
                      Hallucination ? "normal" : "strange");
            else if (tp_sensemon(mtmp))
                pline(msgc, "%s disappears, but you can still %s.", nambuf,
                      Hallucination ? "see its aura" : "sense its thoughts");
            else if (canspotmon(mtmp)) /* e.g. the orc/Sting case */
                pline(msgc, "%s %s %s.", s_suffix(nambuf),
                      mbodypart(mtmp, BODY),
                      Hallucination ? "is totally psychedelic" :
                      "seems to lose its definition");
            else
                pline(msgc, "Suddenly you cannot see %s.", nambuf);
            if (oseen)
                makeknown(otmp->otyp);
        }
        if (otmp->otyp == POT_INVISIBILITY) {
            if (otmp->cursed)
                you_aggravate(mtmp);
            m_useup(mtmp, otmp);
        }
        return 2;
    case MUSE_POT_SPEED:
        mquaffmsg(mtmp, otmp);
        /* note difference in potion effect due to substantially different
           methods of maintaining speed ratings: player's character becomes
           "very fast" temporarily; monster becomes "one stage faster"
           permanently */
        mon_adjust_speed(mtmp, 1, otmp);
        if (oseen)
            makeknown(otmp->otyp);
        m_useup(mtmp, otmp);
        if (oseen)
            makeknown(otmp->otyp);
        return 2;
    case MUSE_POT_POLYMORPH:
        mquaffmsg(mtmp, otmp);
        if (vismon)
            pline(msgc, "%s suddenly mutates!", Monnam(mtmp));
        newcham(mtmp, muse_newcham_mon(mtmp), FALSE, FALSE);
        if (oseen)
            makeknown(POT_POLYMORPH);
        m_useup(mtmp, otmp);
        return 2;
    case MUSE_POLY_TRAP:
        if (vismon) {
            /* If the player can see the monster jump onto a square and
               polymorph, they'll know there's a trap there even if they can't
               see the square the trap's on (e.g. infravisible monster). */
            pline(msgc, "%s deliberately %s onto a polymorph trap!",
                  Monnam(mtmp), makeplural(locomotion(mtmp->data, "jump")));
            seetrap(t_at(level, trapx, trapy));
        }

        /* don't use rloc() due to worms */
        remove_monster(level, mtmp->mx, mtmp->my);
        newsym(mtmp->mx, mtmp->my);
        place_monster(mtmp, trapx, trapy);
        if (mtmp->wormno)
            worm_move(mtmp);
        newsym(trapx, trapy);

        newcham(mtmp, NULL, FALSE, FALSE);
        return 2;
    case MUSE_BULLWHIP:
        /* attempt to disarm hero */
        if (uwep && !rn2(5)) {
            const char *The_whip = vismon ? "The bullwhip" : "A whip";
            int where_to = rn2(4);
            struct obj *obj = uwep;
            const char *hand;
            const char *the_weapon = the(xname(obj));

            hand = body_part(HAND);
            if (bimanual(obj) && mtmp->data->msize < MZ_HUGE)
                hand = makeplural(hand);

            if (obj->otyp == HEAVY_IRON_BALL) {
                if (vismon)
                    pline(combat_msgc(mtmp, &youmonst, cr_immune),
                          "%s tries to wrap a whip around your iron ball.  "
                          "Fat chance.",
                          Monnam(mtmp));
                else
                    pline(msgc_playerimmune,
                          "A whip fails to wrap around your iron ball.");
                return 1;
            }
            pline(combat_msgc(mtmp, &youmonst, cr_hit),
                  "%s wraps around %s you're wielding!",
                  The_whip, the_weapon);
            if (welded(obj)) {
                pline(combat_msgc(mtmp, &youmonst, cr_immune),
                      "%s %s your %s%c",
                      !is_plural(obj) ? "It is" : "They are",
                      (objects[obj->otyp].oc_material == WOOD) ?
                      "grown right into" : "welded to",
                      hand, !obj->bknown ? '!' : '.');
                /* obj->bknown = 1; *//* welded() takes care of this */
                where_to = 0;
            }
            if ((obj->otyp == LEASH) && (obj->leashmon != 0)) {
                struct monst *pet = find_mid(level, obj->leashmon, FM_FMON);
                pline(msgc_combatgood, "%s becomes entangled in %s leash.",
                      The_whip, (pet ? mhis(pet) : "your"));
                yelp(pet);
                setmnotwielded(mtmp, otmp);
                obj_extract_self(otmp);
                MON_NOWEP(mtmp);
                otmp->owornmask = 0L;
                pline(msgc_combatgood, "%s tugs away from %s.", The_whip,
                      (vismon ? mon_nam(mtmp) : "something"));
                otmp = hold_another_object(otmp, "%s untangles and falls away.",
                                           doname(otmp), "You now have ");
                return 1;
            } else if (!where_to) {
                pline(msgc_statusheal,
                      "The whip slips free.");  /* not `The_whip' */
                return 1;
            }
            if (vismon)
                pline_implied(combat_msgc(mtmp, &youmonst, cr_hit),
                              "%s flicks a bullwhip towards your %s!",
                              Monnam(mtmp), hand);
            else /* not msgc_itemloss, that'll appear in a later message */
                pline_implied(msgc_moncombatbad,
                              "A whip wraps around %s you're wielding!",
                              the_weapon);

            if (where_to == 3 &&
                hates_material(mtmp->data, objects[obj->otyp].oc_material)) {
                /* this monster won't want to catch a silver weapon; drop it at 
                   hero's feet instead */
                where_to = 2;
            }
            uwepgone();
            freeinv(obj);
            switch (where_to) {
            case 1:    /* onto floor beneath mon */
                pline(msgc_itemloss, "%s yanks %s from your %s!",
                      Monnam(mtmp), the_weapon, hand);
                place_object(obj, level, mtmp->mx, mtmp->my);
                break;
            case 2:    /* onto floor beneath you */
                pline(msgc_itemloss, "%s yanks %s to the %s!",
                      Monnam(mtmp), the_weapon, surface(u.ux, u.uy));
                dropy(obj);
                break;
            case 3:    /* into mon's inventory */
                pline(msgc_itemloss, "%s snatches %s!",
                      Monnam(mtmp), the_weapon);
                mpickobj(mtmp, obj);
                break;
            }
            return 1;
        }
        return 0;
    case MUSE_SCR_REMOVE_CURSE:
        mreadmsg(mtmp, otmp);
        if (canseemon(mtmp)) {
            if (mtmp->mconf)
                pline(msgc, "You feel as if %s needs some help.",
                      mon_nam(mtmp));
            else
                pline(msgc, "You feel like someone is helping %s.",
                      mon_nam(mtmp));
            if (!objects[SCR_REMOVE_CURSE].oc_name_known &&
                !objects[SCR_REMOVE_CURSE].oc_uname)
                docall(otmp);
        }
        {
            struct obj *obj;

            for (obj = mtmp->minvent; obj; obj = obj->nobj) {
                /* gold isn't subject to cursing and blessing */
                if (obj->oclass == COIN_CLASS)
                    continue;
                if (otmp->blessed || otmp->owornmask ||
                    obj->otyp == LOADSTONE) {
                    if (mtmp->mconf)
                        blessorcurse(obj, 2, rng_main);
                    else
                        uncurse(obj);
                }
            }
        }
        m_useup(mtmp, otmp);
        return 0;
    case 0:
        return 0;       /* i.e. an exploded wand */
    default:
        impossible("%s wanted to perform action %d?", Monnam(mtmp),
                   m->has_misc);
        break;
    }
    return 0;
}

void
you_aggravate(struct monst *mtmp)
{
    pline(msgc_youdiscover, "For some reason, %s presence is known to you.",
          s_suffix(noit_mon_nam(mtmp)));
    cls();
    dbuf_set(mtmp->mx, mtmp->my, S_unexplored, 0, 0, 0, 0,
             dbuf_monid(mtmp, mtmp->mx, mtmp->my, rn2), 0, 0, 0);
    display_self();
    
    /* msgc_info is used for detect monster results, so makes sense here */
    pline_implied(msgc_info, "You feel aggravated at %s.", noit_mon_nam(mtmp));
    win_pause_output(P_MAP);
    doredraw();
    cancel_helplessness(hm_unconscious,
                        "Aggravated, you are jolted into full consciousness.");

    newsym(mtmp->mx, mtmp->my);
    if (!canspotmon(mtmp))
        map_invisible(mtmp->mx, mtmp->my);
}

int
rnd_misc_item(struct monst *mtmp, enum rng rng)
{
    const struct permonst *pm = mtmp->data;
    int difficulty = MONSTR(monsndx(pm));

    if (is_animal(pm) || attacktype(pm, AT_EXPL) || mindless(mtmp->data)
        || noncorporeal(pm) || pm->mlet == mtmp->iskop)
        return 0;
    /* Unlike other rnd_item functions, we only allow _weak_ monsters to have
       this item; after all, the item will be used to strengthen the monster
       and strong monsters won't use it at all... */
    if (difficulty < 6 && !rn2_on_rng(30, rng))
        return rn2_on_rng(6, rng) ? POT_POLYMORPH : WAN_POLYMORPH;

    if (!rn2_on_rng(40, rng) && !nonliving(pm))
        return AMULET_OF_LIFE_SAVING;

    switch (rn2_on_rng(3, rng)) {
    case 0:
        if (mtmp->isgd)
            return 0;
        return rn2_on_rng(6, rng) ? POT_SPEED : WAN_SPEED_MONSTER;
    case 1:
        return rn2_on_rng(6, rng) ? POT_INVISIBILITY : WAN_MAKE_INVISIBLE;
    case 2:
        return POT_GAIN_LEVEL;
    }
     /*NOTREACHED*/ return 0;
}

boolean
searches_for_item(struct monst *mon, struct obj *obj)
{
    int typ = obj->otyp;

    /* don't loot bones piles */
    if (is_animal(mon->data) || mindless(mon->data) ||
        mon->data == &mons[PM_GHOST])
        return FALSE;

    if (typ == WAN_MAKE_INVISIBLE || typ == POT_INVISIBILITY)
        return (boolean) (!mon->minvis && !mon->invis_blkd &&
                          !attacktype(mon->data, AT_GAZE));
    if (typ == WAN_SPEED_MONSTER || typ == POT_SPEED)
        return (boolean) (mon->mspeed != MFAST);

    switch (obj->oclass) {
    case WAND_CLASS:
        if (obj->spe <= 0)
            return FALSE;
        if (typ == WAN_DIGGING)
            return (boolean) (!is_floater(mon->data));
        if (typ == WAN_POLYMORPH)
            return (boolean) (MONSTR(monsndx(mon->data)) < 6);
        if (objects[typ].oc_dir == RAY || typ == WAN_STRIKING ||
            typ == WAN_TELEPORTATION || typ == WAN_CREATE_MONSTER)
            return TRUE;
        break;
    case POTION_CLASS:
        if (typ == POT_HEALING || typ == POT_EXTRA_HEALING ||
            typ == POT_FULL_HEALING || typ == POT_POLYMORPH ||
            typ == POT_GAIN_LEVEL || typ == POT_PARALYSIS || typ == POT_SLEEPING
            || typ == POT_ACID || typ == POT_CONFUSION)
            return TRUE;
        if (typ == POT_BLINDNESS && !attacktype(mon->data, AT_GAZE))
            return TRUE;
        break;
    case SCROLL_CLASS:
        if (typ == SCR_TELEPORTATION || typ == SCR_CREATE_MONSTER ||
            typ == SCR_EARTH || typ == SCR_REMOVE_CURSE)
            return TRUE;
        break;
    case AMULET_CLASS:
        if (typ == AMULET_OF_LIFE_SAVING)
            return (boolean) (!nonliving(mon->data));
        if (typ == AMULET_OF_REFLECTION)
            return TRUE;
        break;
    case TOOL_CLASS:
        if (typ == PICK_AXE)
            return (boolean) needspick(mon->data);
        if (typ == UNICORN_HORN)
            return (boolean) (!obj->cursed && !is_unicorn(mon->data));
        if (typ == FROST_HORN || typ == FIRE_HORN)
            return (obj->spe > 0) && can_blow_instrument(mon->data);
        break;
    case FOOD_CLASS:
        if (typ == CORPSE)
            return (boolean) (((mon->misc_worn_check & W_MASK(os_armg)) &&
                               touch_petrifies(&mons[obj->corpsenm])) ||
                              (!resists_ston(mon) &&
                               (obj->corpsenm == PM_LIZARD ||
                                (acidic(&mons[obj->corpsenm]) &&
                                 obj->corpsenm != PM_GREEN_SLIME))));
        if (typ == EGG)
            return (boolean) (touch_petrifies(&mons[obj->corpsenm]));
        break;
    default:
        break;
    }

    return FALSE;
}

/* magr = monster whose attack or refection is being reflected; currently
   accepts NULL because zap.c is too spaghetti to figure it out, but please try
   to avoid this case if you can

   recursive = TRUE if we're testing to see if a reflection is itself
   reflected. This is based on whether magr /intended/ to perform the attack,
   so using a mirror gives recursive = FALSE. */
boolean
mon_reflects(struct monst *mon, struct monst *magr,
             boolean recursive, const char *str)
{
    struct obj *orefl = which_armor(mon, os_arms);
    enum combatresult cr = recursive ? cr_miss : cr_immune;
    
    if (orefl && orefl->otyp == SHIELD_OF_REFLECTION) {
        if (str) {
            pline(combat_msgc(magr, mon, cr),
                  str, s_suffix(mon_nam(mon)), "shield");
            makeknown(SHIELD_OF_REFLECTION);
        }
        return TRUE;
    } else if (arti_reflects(MON_WEP(mon))) {
        /* due to wielded artifact weapon */
        if (str)
            pline(combat_msgc(magr, mon, cr),
                  str, s_suffix(mon_nam(mon)), "weapon");
        return TRUE;
    } else if ((orefl = which_armor(mon, os_amul)) &&
               orefl->otyp == AMULET_OF_REFLECTION) {
        if (str) {
            pline(combat_msgc(magr, mon, cr),
                  str, s_suffix(mon_nam(mon)), "amulet");
            makeknown(AMULET_OF_REFLECTION);
        }
        return TRUE;
    } else if ((orefl = which_armor(mon, os_arm)) &&
               (orefl->otyp == SILVER_DRAGON_SCALES ||
                orefl->scalecolor == DRAGONCOLOR_SILVER)) {
        if (str)
            pline(combat_msgc(magr, mon, cr),
                  str, s_suffix(mon_nam(mon)), "armor");
        return TRUE;
    } else if (mon->data == &mons[PM_SILVER_DRAGON] ||
               mon->data == &mons[PM_GREAT_FIERCE_BEAST]) {
        /* Silver dragons only reflect when mature; babies do not */
        if (str)
            pline(combat_msgc(magr, mon, cr),
                  str, s_suffix(mon_nam(mon)), "scales");
        return TRUE;
    }
    return FALSE;
}

/* TODO: merge with mon_reflects (why does it have a totally different API? */
boolean
ureflects(const char *fmt, const char *str)
{
    /* Check from outermost to innermost objects */
    unsigned reflect_reason = u_have_property(REFLECTING, ANY_PROPERTY, FALSE);
    if (reflect_reason & W_MASK(os_arms)) {
        if (fmt && str) {
            pline(msgc_playerimmune, fmt, str, "shield");
            makeknown(SHIELD_OF_REFLECTION);
        }
        return TRUE;
    } else if (reflect_reason & W_MASK(os_wep)) {
        /* Due to wielded artifact weapon */
        if (fmt && str)
            pline(msgc_playerimmune, fmt, str, "weapon");
        return TRUE;
    } else if (reflect_reason & W_MASK(os_amul)) {
        if (fmt && str) {
            pline(msgc_playerimmune, fmt, str, "medallion");
            makeknown(AMULET_OF_REFLECTION);
        }
        return TRUE;
    } else if (reflect_reason & W_MASK(os_arm)) {
        if (fmt && str)
            pline(msgc_playerimmune, fmt, str, "armor");
        return TRUE;
    } else if (reflect_reason & W_MASK(os_polyform)) {
        if (fmt && str)
            pline(msgc_playerimmune, fmt, str, "scales");
        return TRUE;
    } else if (reflect_reason) {
        impossible("Reflecting for unknown reason");
        return TRUE;
    }
    return FALSE;
}


/* TRUE if the monster ate something */
boolean
munstone(struct monst * mon, boolean by_you)
{
    struct obj *obj;

    if (resists_ston(mon))
        return FALSE;
    if (mon->meating || !mon->mcanmove || mon->msleeping)
        return FALSE;

    for (obj = mon->minvent; obj; obj = obj->nobj) {
        /* Monsters can also use potions of acid */
        if ((obj->otyp == POT_ACID) ||
            (obj->otyp == CORPSE &&
             (obj->corpsenm == PM_LIZARD ||
              (acidic(&mons[obj->corpsenm]) &&
               obj->corpsenm != PM_GREEN_SLIME)))) {
            mon_consume_unstone(mon, obj, by_you, TRUE);
            return TRUE;
        }
    }
    return FALSE;
}

/* TODO: Give access to the attacking monster (not just by_you), so that pets
   can channelize as petkill if their target dies trying to save itself */
static void
mon_consume_unstone(struct monst *mon, struct obj *obj, boolean by_you,
                    boolean stoning)
{
    int nutrit = (obj->otyp == CORPSE) ? dog_nutrition(mon, obj) : 0;

    /* also sets meating */

    /* give a "<mon> is slowing down" message and also remove intrinsic speed
       (comparable to similar effect on the hero) */
    if (stoning)
        mon_adjust_speed(mon, -3, NULL);

    if (mon_visible(mon)) {
        long save_quan = obj->quan;

        obj->quan = 1L;
        pline(mon->mtame ? msgc_petwarning : msgc_monneutral, "%s %ss %s.",
              Monnam(mon), (obj->otyp == POT_ACID) ? "quaff" : "eat",
              distant_name(obj, doname));
        obj->quan = save_quan;
    } else
        You_hear(msgc_levelsound, "%s.",
                 (obj->otyp == POT_ACID) ? "drinking" : "chewing");
    if (((obj->otyp == POT_ACID) || acidic(&mons[obj->corpsenm])) &&
        !resists_acid(mon)) {
        mon->mhp -= rnd(15);
        pline(mon->mtame ? msgc_petwarning :
              by_you ? msgc_consequence : msgc_monneutral,
              "%s has a very bad case of stomach acid.", Monnam(mon));
    }
    if (mon->mhp <= 0) {
        pline(by_you ? msgc_kill : mon->mtame ?
              msgc_petfatal : msgc_monneutral, "%s dies!", Monnam(mon));
        m_useup(mon, obj);
        if (by_you)
            xkilled(mon, 0);
        else
            mondead(mon);
        return;
    }
    if (stoning && canseemon(mon)) {
        if (Hallucination)
            pline(combat_msgc(mon, NULL, cr_hit),
                  "What a pity - %s just ruined a future piece of art!",
                  mon_nam(mon));
        else
            pline(combat_msgc(mon, NULL, cr_hit),
                  "%s seems limber!", Monnam(mon));
    }
    if (obj->otyp == CORPSE && obj->corpsenm == PM_LIZARD && mon->mconf) {
        mon->mconf = 0;
        if (canseemon(mon))
            pline(combat_msgc(mon, NULL, cr_hit),
                  "%s seems steadier now.", Monnam(mon));
    }
    if (mon->mtame && !mon->isminion && nutrit > 0) {
        struct edog *edog = EDOG(mon);

        if (edog->hungrytime < moves)
            edog->hungrytime = moves;
        edog->hungrytime += nutrit;
        mon->mconf = 0;
    }
    mon->mlstmv = moves;        /* it takes a turn */
    m_useup(mon, obj);
}

/*muse.c*/

