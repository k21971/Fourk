/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-11-11 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

boolean notonhead = FALSE;

static int nothing, unkn;
static const char beverages_and_fountains[] =
    { ALLOW_NONE, NONE_ON_COMMA, POTION_CLASS, 0 };
static const char beverages[] = { POTION_CLASS, 0 };

static long itimeout(long);
static long itimeout_incr(long, int);
static void ghost_from_bottle(void);
static int overheal_amount(int);
static short mixtype(struct obj *, struct obj *);

/* force `val' to be within valid range for intrinsic timeout value */
static long
itimeout(long val)
{
    if (val >= TIMEOUT)
        val = TIMEOUT;
    else if (val < 1)
        val = 0;

    return val;
}

/* increment `old' by `incr' and force result to be valid intrinsic timeout */
static long
itimeout_incr(long old, int incr)
{
    return itimeout((old & TIMEOUT) + (long)incr);
}

/* set the timeout field of intrinsic `which' */
void
set_itimeout(unsigned int *which, long val)
{
    *which &= ~TIMEOUT;
    *which |= itimeout(val);
}

/* increment the timeout field of intrinsic `which' */
void
incr_itimeout(unsigned int *which, long incr)
{
    set_itimeout(which, itimeout_incr(*which, incr));
}


void
make_confused(long xtime, boolean talk)
{
    long old = HConfusion;

    if (!xtime && old) {
        if (talk)
            pline(msgc_statusheal, "You feel less %s now.",
                  Hallucination ? "trippy" : "confused");
    }

    set_itimeout(&HConfusion, xtime);
}


void
make_stunned(long xtime, boolean talk)
{
    long old = HStun;

    if (!xtime && old) {
        if (talk)
            pline(msgc_statusheal, "You feel %s now.",
                  Hallucination ? "less wobbly" : "a bit steadier");
    }
    if (xtime && !old) {
        if (talk) {
            if (u.usteed)
                pline(msgc_statusbad, "You wobble in the saddle.");
            else
                pline(msgc_statusbad, "You %s...",
                      stagger(youmonst.data, "stagger"));
        }
    }

    set_itimeout(&HStun, xtime);
}


void
make_sick(long xtime, const char *cause, boolean talk, int type)
{
    long old = Sick;

    if (xtime > 0L) {
        if (Sick_resistance)
            return;
        if (!old) {
            /* newly sick */
            pline(msgc_fatal, "You feel deathly sick.");
        } else {
            /* already sick */
            if (talk)
                pline(msgc_fatal, "You feel %s worse.",
                      xtime <= Sick / 2L ? "much" : "even");
        }
        set_itimeout(&Sick, xtime);
        u.usick_type |= type;
    } else if (old && (type & u.usick_type)) {
        /* was sick, now not */
        u.usick_type &= ~type;
        if (u.usick_type) {     /* only partly cured */
            /* channelization note: use a "something went wrong" channel, not
               a "healing" channel, to increase the chance the player notices
               the delayed instadeath is still there */
            if (talk)
                pline(msgc_substitute, "You feel somewhat better.");
            set_itimeout(&Sick, Sick * 2);      /* approximation */
        } else {
            if (talk)
                pline(msgc_statusheal, "What a relief!");
            Sick = 0L;  /* set_itimeout(&Sick, 0L) */
        }
    }

    if (Sick) {
        exercise(A_CON, FALSE);
        if (cause) {
            set_delayed_killer(POISONING, killer_msg(POISONING, cause));
        } else
            set_delayed_killer(POISONING, NULL);
    } else
        set_delayed_killer(POISONING, NULL);
}


void
make_vomiting(long xtime, boolean talk)
{
    long old = Vomiting;

    if (!xtime && old)
        if (talk)
            pline(msgc_statusheal, "You feel much less nauseated now.");

    set_itimeout(&Vomiting, xtime);
}

static const char vismsg[] =
    "Your vision seems to %s for a moment but is %s now.";
static const char eyemsg[] = "Your %s momentarily %s.";

static void
eyepline(const char *verb_one_eye, const char *verb_two_eyes)
{
    if (eyecount(youmonst.data) == 1)
        pline(msgc_playerimmune, eyemsg, body_part(EYE), verb_one_eye);
    else
        pline(msgc_playerimmune, eyemsg, makeplural(body_part(EYE)),
              verb_two_eyes);
}

void
make_blinded(long xtime, boolean talk)
{
    long old = Blinded;
    boolean u_could_see, can_see_now;

    /* we need to probe ahead in case the Eyes of the Overworld are or will be
       overriding blindness */
    u_could_see = !Blind;
    Blinded = xtime ? 1L : 0L;
    can_see_now = !Blind;
    Blinded = old;      /* restore */

    if (u_helpless(hm_unconscious))
        talk = FALSE;

    if (can_see_now && !u_could_see) {  /* regaining sight */
        if (talk) {
            if (Hallucination)
                pline(msgc_statusheal, "Far out!  A light show!");
            else
                pline(msgc_statusheal, "You can see again.");
        }
    } else if (old && !xtime) {
        /* clearing temporary blindness without toggling blindness */
        if (talk) {
            if (!haseyes(youmonst.data)) {
                strange_feeling(NULL, NULL);
            } else if (Blindfolded) {
                eyepline("itches", "itch");
            } else {    /* Eyes of the Overworld */
                pline(msgc_playerimmune, vismsg, "brighten",
                      Hallucination ? "sadder" : "normal");
            }
        }
    }

    if (u_could_see && !can_see_now) {  /* losing sight */
        if (talk) {
            if (Hallucination)
                pline(msgc_statusbad,
                      "Oh, bummer!  Everything is dark!  Help!");
            else
                pline(msgc_statusbad, "A cloud of darkness falls upon you.");
        }
        /* Before the hero goes blind, set the ball&chain variables. */
        if (Punished)
            set_bc(0);
    } else if (!old && xtime) {
        /* setting temporary blindness without toggling blindness */
        if (talk) {
            if (!haseyes(youmonst.data)) {
                strange_feeling(NULL, NULL);
            } else if (Blindfolded) {
                eyepline("twitches", "twitch");
            } else {    /* Eyes of the Overworld */
                pline(msgc_playerimmune, vismsg, "dim",
                      Hallucination ? "happier" : "normal");
            }
        }
    }

    set_itimeout(&Blinded, xtime);

    if (u_could_see ^ can_see_now) {    /* one or the other but not both */
        turnstate.vision_full_recalc = TRUE;
        /* This vision recalculation used to be deferred until moveloop(), but
           that made it possible for vision irregularities to occur (cited case
           was force bolt hitting adjacent potion of blindness and then a secret
           door; hero was blinded by vapors but then got the message "a door
           appears in the wall", C343-99) */
        vision_recalc(0);
        if (Blind_telepat || Infravision)
            see_monsters(FALSE);
    }
}


boolean
make_hallucinated(long xtime,   /* nonzero if this is an attempt to turn on
                                   hallucination */
                  boolean talk)
{       /* nonzero if resistance status should change by mask */
    long old = HHallucination;
    boolean changed = 0;
    const char *message, *verb;

    message =
        (!xtime) ? "Everything %s SO boring now." :
        "Oh wow!  Everything %s so cosmic!";
    verb = (!Blind) ? "looks" : "feels";

    if (!Halluc_resistance && (! !HHallucination != ! !xtime))
        changed = TRUE;
    set_itimeout(&HHallucination, xtime);

    /* clearing temporary hallucination without toggling vision */
    if (!changed && !HHallucination && old && talk) {
        if (!haseyes(youmonst.data)) {
            strange_feeling(NULL, NULL);
        } else if (Blind) {
            eyepline("itches", "itch");
        } else {    /* Grayswandir */
            pline(msgc_playerimmune, vismsg, "flatten", "normal");
        }
    }

    if (changed && !program_state.suppress_screen_updates) {
        if (Engulfed) {
            swallowed(0);       /* redraw swallow display */
        } else {
            /* The see_* routines should be called *before* the pline. */
            see_monsters(TRUE);
            see_objects(TRUE);
            see_traps(TRUE);
        }

        /* for perm_inv and anything similar (eg. Qt windowport's equipped
           items display) */
        update_inventory();

        if (talk)
            pline(xtime ? msgc_statusbad : msgc_statusheal, message, verb);
    }
    if (changed && !Hallucination && !uarmg && uwep && uwep->otyp == CORPSE &&
        touch_petrifies(&mons[uwep->corpsenm]) && !Stone_resistance) {
        pline(msgc_fatal_predone, "You touch the %s corpse and realize %s.",
              mons[uwep->corpsenm].mname, "that you are still stoned");
        instapetrify(killer_msg(STONING,
                                msgprintf("touching %s corpse",
                                          an(mons[uwep->corpsenm].mname))));
        /* life-saved: unwield the corpse if we can't handle it */
        if (!Stone_resistance && !uarmg && !Hallucination)
            uwepgone();
    }
    if (changed && Hallucination && Stoned) {
        /* Alternate messages:  "Now you are totally stoned!"
                                "Now you really ARE stoned!"
                                "You feel totally loose!" */
        pline(msgc_statusheal, "Wow, being stoned isn't so bad after all!");
        fix_petrification();
    }

    return changed;
}

static void
ghost_from_bottle(void)
{
    struct monst *mtmp =
        makemon(&mons[PM_GHOST], level, u.ux, u.uy, NO_MM_FLAGS);

    if (!mtmp) {
        pline(msgc_failrandom, "This bottle turns out to be empty.");
        return;
    }
    if (Blind || !See_invisible) {
        pline(msgc_substitute, "As you open the bottle, something emerges.");
        return;
    }
    if (Hallucination) {
        int idx = rndmonidx();

        pline(msgc_substitute, "As you open the bottle, %s emerges!",
              monnam_is_pname(idx) ? monnam_for_index(idx)
              : (idx < SPECIAL_PM && (mons[idx].geno & G_UNIQ))
              ? the(monnam_for_index(idx))
              : an(monnam_for_index(idx)));
    } else {
        pline(msgc_substitute,
              "As you open the bottle, an enormous ghost emerges!");
    }
    pline_implied(msgc_statusbad,
                  "You are frightened to death, and unable to move.");

    helpless(3, hr_afraid, "being frightened to death",
             "You regain your composure.");
}


/* "Quaffing is like drinking, except you spill more." -- Terry Pratchett */
int
dodrink(const struct nh_cmd_arg *arg)
{
    struct obj *potion;
    const char *potion_descr;
    void (*terrain) (void) = 0;

    if (Strangled) {
        pline(msgc_cancelled,
              "If you can't breathe air, how can you drink liquid?");
        return 0;
    }
    /* Is there a fountain to drink from here? */
    if (IS_FOUNTAIN(level->locations[u.ux][u.uy].typ) && !Engulfed &&
        !Levitation) {
        terrain = drinkfountain;
    }
    /* Or a kitchen sink? */
    if (IS_SINK(level->locations[u.ux][u.uy].typ) && !Engulfed) {
        terrain = drinksink;
    }

    /* Or are you surrounded by water? */
    if (((is_puddle(level, u.ux, u.uy) && !verysmall(youmonst.data)) ||
         (is_pool(level, u.ux, u.uy) && Wwalking)) && !Engulfed) {
        if (yn(msgprintf("Drink the water at your %s?",
                         makeplural(body_part(FOOT))))) {
            if ((youmonst.data->mlet == S_JELLY) ||
                (youmonst.data->mlet == S_PUDDING))
                pline(msgc_cancelled1,
                      "If there's anything you could absorb that would make "
                      "you any slimier, it's probably living in that water.");
            else
                pline(msgc_cancelled1, "Do you know what lives in that water?");
        }
    } else if ((Underwater ||
                (is_puddle(level, u.ux, u.uy) && verysmall(youmonst.data)))
               && !Engulfed) {
        if (yn("Drink the water around you?") == 'y') {
            pline(msgc_cancelled1, "Do you know what lives in this water?");
            return 1;
        }
    }

    potion = getargobj(arg, terrain ? beverages_and_fountains : beverages,
                       "drink");
    if (!potion)
        return 0;

    if (potion == &zeroobj) {
        if (!terrain)
            return 0;   /* sanity; should be impossible */
        terrain();
        return 1;
    }

    potion->in_use = TRUE;      /* you've opened the stopper */

#define POTION_OCCUPANT_CHANCE(n) (13 + 2*(n))  /* also in muse.c */

    potion_descr = OBJ_DESCR(objects[potion->otyp]);
    if (potion_descr) {
        if (!strcmp(potion_descr, "milky") && flags.ghost_count < MAXMONNO &&
            !rn2(POTION_OCCUPANT_CHANCE(flags.ghost_count))) {
            ghost_from_bottle();
            break_conduct(conduct_potions);
            useup(potion);
            return 1;
        } else if (!strcmp(potion_descr, "smoky") &&
                   flags.djinni_count < MAXMONNO &&
                   !rn2_on_rng(POTION_OCCUPANT_CHANCE(flags.djinni_count),
                               rng_smoky_potion)) {
            djinni_from_bottle(potion);
            break_conduct(conduct_potions);
            useup(potion);
            return 1;
        }
    }
    return dopotion(potion);
}


int
dopotion(struct obj *otmp)
{
    int retval;

    if (otmp == uwep) {
        /* Unwield the potion to avoid a crash if its effect causes the player
           to drop it. We don't print a message here; setuwep doesn't either. */
        setuwep(0);
    }

    if (objects[otmp->otyp].oc_magic)
        break_conduct(conduct_potions);
    otmp->in_use = TRUE;
    nothing = unkn = 0;
    if ((retval = peffects(otmp)) >= 0)
        return retval;

    if (nothing) {
        unkn++;
        pline(msgc_nospoil,
              "You have a %s feeling for a moment, then it passes.",
              Hallucination ? "normal" : "peculiar");
    }
    if (otmp->dknown && !objects[otmp->otyp].oc_name_known) {
        if (!unkn) {
            makeknown(otmp->otyp);
            more_experienced(0, 10);
        } else if (!objects[otmp->otyp].oc_uname)
            docall(otmp);
    }
    useup(otmp);
    return 1;
}

static int
overheal_amount(int maxhp)
{
    int max = (250 - maxhp) / 5;
    if ((max <= 0) || (ilog2(max) <= 0))
        return 0;
    return rnd(ilog2(max)) / 1000;
}

int
peffects(struct obj *otmp)
{
    int i, ii, lim;

    switch (otmp->otyp) {
    case POT_RESTORE_ABILITY:
    case SPE_RESTORE_ABILITY:
        unkn++;
        if (otmp->cursed) {
            pline(msgc_statusbad, "Ulch!  This makes you feel mediocre!");
            break;
        } else {
            /* unlike unicorn horn, overrides Fixed_abil */
            pline(msgc_statusheal, "Wow!  This makes you feel %s!",
                  (otmp->blessed) ? (unfixable_trouble_count(FALSE) ? "better" :
                                     "great")
                  : "good");
            i = rn2(A_MAX);     /* start at a random point */
            for (ii = 0; ii < A_MAX; ii++) {
                lim = AMAX(i);
                if (i == A_STR && u.uhs >= 3)
                    --lim;      /* WEAK */
                if (ABASE(i) < lim) {
                    ABASE(i) = lim;
                    /* only first found if not blessed */
                    if (!otmp->blessed)
                        break;
                }
                if (++i >= A_MAX)
                    i = 0;
            }
        }
        break;
    case POT_HALLUCINATION:
        if (Hallucination || Halluc_resistance)
            nothing++;
        make_hallucinated(itimeout_incr
                          (HHallucination, rn1(200, 600 - 300 * bcsign(otmp))),
                          TRUE);
        break;
    case POT_WATER:
        if (!otmp->blessed && !otmp->cursed) {
            pline(msgc_actionok, "This tastes like water.");
            u.uhunger += rnd(10);
            newuhs(FALSE);
            break;
        }
        unkn++;
        if (is_undead(youmonst.data) || is_demon(youmonst.data) ||
            u.ualign.type == A_CHAOTIC) {
            if (otmp->blessed) {
                pline(msgc_badidea, "This burns like acid!");
                exercise(A_CON, FALSE);
                if (u.ulycn >= LOW_PM) {
                    pline(msgc_statusheal, "Your affinity to %s disappears!",
                          makeplural(mons[u.ulycn].mname));
                    if (youmonst.data == &mons[u.ulycn])
                        you_unwere(FALSE);
                    u.ulycn = NON_PM;   /* cure lycanthropy */
                }
                losehp(dice(2, 6), killer_msg(DIED, "a potion of holy water"));
            } else if (otmp->cursed) {
                pline(msgc_statusheal, "You feel quite proud of yourself.");
                healup(dice(2, 6), 0, 0, 0);
                if (u.ulycn >= LOW_PM && !Upolyd)
                    you_were();
                exercise(A_CON, TRUE);
            }
        } else {
            if (otmp->blessed) {
                pline(msgc_statusheal, "You feel full of awe.");
                make_sick(0L, NULL, TRUE, SICK_ALL);
                exercise(A_WIS, TRUE);
                exercise(A_CON, TRUE);
                if (u.ulycn >= LOW_PM)
                    you_unwere(TRUE);   /* "Purified" */
                /* make_confused(0L,TRUE); */
            } else {
                if (u.ualign.type == A_LAWFUL) {
                    pline(msgc_badidea, "This burns like acid!");
                    losehp(dice(2, 6),
                           killer_msg(DIED, "a potion of unholy water"));
                } else
                    pline(msgc_actionok, "You feel full of dread.");
                if (u.ulycn >= LOW_PM && !Upolyd)
                    you_were();
                exercise(A_CON, FALSE);
            }
        }
        break;
    case POT_BOOZE:
        unkn++;
        pline(msgc_statusbad, "Ooph!  This tastes like %s%s!",
              otmp->odiluted ? "watered down " : "",
              Hallucination ? "dandelion wine" : "liquid fire");
        if (!otmp->blessed) {
            /* per entrez via PatR (e44de714), booze hits harder if drinking on
               an empty stomach */
            make_confused(itimeout_incr(HConfusion, dice(2 + u.uhs, otmp->odiluted ? 5 : 8)), FALSE);
        }
        /* the whiskey makes us feel better */
        if (!otmp->odiluted)
            healup(1, 0, FALSE, FALSE);
        u.uhunger += 10 * (2 + bcsign(otmp));
        newuhs(FALSE);
        if (otmp->cursed) {
            pline(msgc_statusbad, "You pass out.");
            helpless(rnd(15), hr_fainted, "drunk",
                     "You awake with a headache.");
            see_monsters(FALSE);
            see_objects(FALSE);
            turnstate.vision_full_recalc = TRUE;
        }
        break;
    case POT_ENLIGHTENMENT:
        if (otmp->cursed) {
            unkn++;
            pline(msgc_failcurse, "You have an uneasy feeling...");
            exercise(A_WIS, FALSE);
        } else {
            if (otmp->blessed) {
                adjattrib(A_INT, 1, FALSE);
                adjattrib(A_WIS, 1, FALSE);
            }
            pline(msgc_info, "You feel self-knowledgeable...");
            win_pause_output(P_MESSAGE);
            enlightenment(0);
            pline_implied(msgc_info, "The feeling subsides.");
            if (otmp->blessed)
                exercise(A_WIS, TRUE);
        }
        break;
    case SPE_INVISIBILITY:
        /* spell cannot penetrate mummy wrapping */
        if (BInvis && uarmc->otyp == MUMMY_WRAPPING) {
            pline(msgc_failcurse, "You feel rather itchy under your %s.",
                  xname(uarmc));
            break;
        }
        /* FALLTHRU */
    case POT_INVISIBILITY:
        if (Invis || Blind || BInvis) {
            nothing++;
        } else {
            self_invis_message();
        }
        {
            int num = 200
                * ((!(otmp->cursed)) ? 2 : 1)
                * ((otmp->blessed) ? 2 : 1);
            incr_itimeout(&HInvis, num + rn2_on_rng(num, rng_potion_duration));
        }
        newsym(u.ux, u.uy);     /* update position */
        if (otmp->cursed) {
            pline(msgc_levelwarning,
                  "For some reason, you feel your presence is known.");
            aggravate();
        }
        break;
    case POT_SIGHT:
        /* tastes like fruit juice in Rogue */
    case POT_FRUIT_JUICE:
        {
            int msg = Invisible && !Blind;

            unkn++;
            if (otmp->cursed)
                pline(msgc_failcurse, "Yecch!  This tastes %s.",
                      Hallucination ? "overripe" : "rotten");
            else
                pline(msgc_actionok, Hallucination ?
                      "This tastes like 10%% real %s%s all-natural beverage." :
                      "This tastes like %s%s.",
                      otmp->odiluted ? "reconstituted " : "", fruitname(TRUE));
            if (otmp->otyp == POT_FRUIT_JUICE) {
                u.uhunger += (otmp->odiluted ? 5 : 10) * (2 + bcsign(otmp));
                newuhs(FALSE);
                break;
            }
            /* Tell them they can see again immediately, which will help
               them identify the potion... */
            make_blinded(0L, TRUE);
            if (Hallucination && !otmp->cursed) {
                pline(msgc_statusheal, "That was way moby!");
                make_hallucinated(0L, FALSE);
            }
            if (otmp->blessed) {
                incr_itimeout(&HSee_invisible,
                              800 + rn2_on_rng(800, rng_potion_duration));
                incr_itimeout(&HXray_vision,
                              150 + rn2_on_rng(50, rng_potion_duration));
            } else {
                int num = 200
                    * ((!(otmp->cursed)) ? 2 : 1)
                    * ((otmp->blessed) ? 2 : 1);
                incr_itimeout(&HSee_invisible,
                              num + rn2_on_rng(num, rng_potion_duration));
            }
            set_mimic_blocking();       /* do special mimic handling */
            see_monsters(FALSE);        /* see invisible monsters */
            newsym(u.ux, u.uy);         /* see yourself! */
            if (msg && !Blind) {        /* Blind possible if polymorphed */
                pline(msgc_intrgain,
                      "You can see through yourself, but you are visible!");
                unkn--;
            }
            break;
        }
    case POT_PARALYSIS:
        if (Free_action)
            pline(msgc_yafm, "You stiffen momentarily.");
        else {
            if (Levitation || Is_airlevel(&u.uz) || Is_waterlevel(&u.uz))
                pline(msgc_statusbad, "You are motionlessly suspended.");
            else if (u.usteed)
                pline(msgc_statusbad, "You are frozen in place!");
            else
                pline(msgc_statusbad, "Your %s are frozen to the %s!",
                      makeplural(body_part(FOOT)), surface(u.ux, u.uy));
            helpless(rn1(10, 25 - 12 * bcsign(otmp)), hr_paralyzed,
                     "frozen by a potion", NULL);
            exercise(A_DEX, FALSE);
        }
        break;
    case POT_SLEEPING:
        if (Sleep_resistance || Free_action)
            pline(msgc_yafm, "You yawn.");
        else {
            pline(msgc_statusbad, "You suddenly fall asleep!");
            helpless(rn1(10, 25 - 12 * bcsign(otmp)), hr_asleep, "sleeping",
                     NULL);
        }
        break;
    case POT_MONSTER_DETECTION:
    case SPE_DETECT_MONSTERS:
        if (otmp->blessed) {
            int x, y;

            if (Detect_monsters)
                nothing++;
            unkn++;
            /* after a while, repeated uses become less effective */
            if (HDetect_monsters >= 300L)
                i = 1;
            else
                i = rn1(40, 21);
            incr_itimeout(&HDetect_monsters, i);
            for (x = 0; x < COLNO; x++) {
                for (y = 0; y < ROWNO; y++) {
                    if (level->locations[x][y].mem_invis) {
                        /* don't clear object memory from below monsters */
                        level->locations[x][y].mem_invis = FALSE;
                        newsym(x, y);
                    }
                    if (MON_AT(level, x, y))
                        unkn = 0;
                }
            }
            see_monsters(FALSE);
            if (unkn)
                pline(msgc_youdiscover, "You feel lonely.");
            break;
        }
        if (monster_detect(otmp, 0))
            return 1;   /* nothing detected */
        break;
    case POT_OBJECT_DETECTION:
    case SPE_DETECT_TREASURE:
        if (object_detect(otmp, 0))
            return 1;   /* nothing detected */
        break;
    case POT_SICKNESS:
        pline(msgc_badidea, "Yecch!  This stuff tastes like poison.");
        if (otmp->blessed) {
            /* fails due to BCU, but in a /good/ way... */
            pline(msgc_failcurse, "(But in fact it was mildly stale %s.)",
                  fruitname(TRUE));
            if (!Poison_resistance && !Role_if(PM_HEALER)) {
                /* NB: blessed otmp->fromsink is not possible */
                losehp(1, killer_msg(DIED, "a mildly contaminated potion"));
            }
        } else {
            if (Poison_resistance)
                pline(msgc_notresisted,
                      "(But in fact it was biologically contaminated %s.)",
                      fruitname(TRUE));
            if (Role_if(PM_HEALER))
                pline(msgc_playerimmune,
                      "Fortunately, you have been immunized.");
            else {
                int typ = rn2(A_MAX);

                if (!Fixed_abil) {
                    poisontell(typ);
                    adjattrib(typ, Poison_resistance ? -1 : -rn1(4, 3), TRUE);
                }
                if (!Poison_resistance) {
                    if (otmp->fromsink)
                        losehp(rnd(10) + 5 * ! !(otmp->cursed),
                               killer_msg(DIED, "contaminated tap water"));
                    else
                        losehp(rnd(10) + 5 * ! !(otmp->cursed),
                               killer_msg(DIED, "a contaminated potion"));
                }
                exercise(A_CON, FALSE);
            }
        }
        if (Hallucination) {
            pline(msgc_statusheal, "You are shocked back to your senses!");
            make_hallucinated(0L, FALSE);
        }
        break;
    case POT_CONFUSION:
        if (!Confusion)
            if (Hallucination) {
                pline(msgc_statusbad, "What a trippy feeling!");
                unkn++;
            } else
                pline(msgc_statusbad, "Huh, What?  Where am I?");
        else
            nothing++;
        make_confused(itimeout_incr(HConfusion, rn1(7, 16 - 8 * bcsign(otmp))),
                      FALSE);
        break;
    case POT_GAIN_ABILITY:
        if (otmp->cursed) {
            pline(msgc_failcurse, "Ulch!  That potion tasted foul!");
            unkn++;
        } else if (Fixed_abil) {
            nothing++;
        } else {        /* If blessed, increase all; if not, try up to */
            int itmp;   /* 6 times to find one which can be increased. */

            i = -1;     /* increment to 0 */
            for (ii = A_MAX; ii > 0; ii--) {
                i = (otmp->blessed ? i + 1 : rn2(A_MAX));
                /* only give "your X is already as high as it can get" message
                   on last attempt (except blessed potions) */
                itmp = (otmp->blessed || ii == 1) ? 0 : -1;
                if (adjattrib(i, 1, itmp) && !otmp->blessed)
                    break;
            }
        }
        break;
    case POT_SPEED:
        if (Wounded_legs && !otmp->cursed && !u.usteed
            /* heal_legs() would heal steeds legs */ ) {
            heal_legs(Wounded_leg_side);
            unkn++;
            break;
        }       /* and fall through */
    case SPE_HASTE_SELF:
        if (!Very_fast) /* wwf@doe.carleton.ca */
            pline(msgc_statusgood, "You are suddenly moving %sfaster.",
                  Fast ? "" : "much ");
        else {
            pline(msgc_statusextend, "Your %s get new energy.",
                  makeplural(body_part(LEG)));
            unkn++;
        }
        exercise(A_DEX, TRUE);
        incr_itimeout(&HFast, rn1(10, 100 + 60 * bcsign(otmp)));
        break;
    case POT_BLINDNESS:
        if (Blind)
            nothing++;
        make_blinded(itimeout_incr(Blinded, rn1(200, 250 - 125 * bcsign(otmp))),
                     (boolean) ! Blind);
        break;
    case POT_GAIN_LEVEL:
        if (otmp->cursed) {
            unkn++;
            /* they went up a level */
            if ((ledger_no(&u.uz) == 1 && Uhave_amulet) ||
                Can_rise_up(u.ux, u.uy, &u.uz)) {
                const char *riseup = "You rise up, through the %s!";

                if (ledger_no(&u.uz) == 1) {
                    pline(msgc_substitute, riseup, ceiling(u.ux, u.uy));
                    goto_level(&earth_level, FALSE, FALSE, FALSE);
                } else {
                    int newlev = depth(&u.uz) - 1;
                    d_level newlevel;

                    get_level(&newlevel, newlev);
                    if (on_level(&newlevel, &u.uz)) {
                        pline(msgc_noconsequence, "It tasted bad.");
                        break;
                    } else
                        pline(msgc_actionok, riseup, ceiling(u.ux, u.uy));
                    goto_level(&newlevel, FALSE, FALSE, FALSE);
                }
            } else
                pline(msgc_yafm, "You have an uneasy feeling.");
            break;
        }
        pluslvl(FALSE);
        if (otmp->blessed)
            /* blessed potions place you at a random spot in the middle of the
               new level instead of the low point */
            u.uexp = rndexp(TRUE);
        break;
    case POT_HEALING:
        if (otmp->blessed && (u.ucramps > 1))
            u.ucramps = u.ucramps * 3 / 4 - 1;
        pline(msgc_statusheal, "You feel better.");
        healup(dice(6 + 2 * bcsign(otmp), 4), 0,
               !!otmp->blessed, !otmp->cursed);
        exercise(A_CON, TRUE);
        break;
    case POT_EXTRA_HEALING:
        if (otmp->blessed && (u.ucramps > 1))
            u.ucramps = u.ucramps * 2 / 3 - 1;
        pline(msgc_statusheal, "You feel much better.");
        healup(dice(6 + 2 * bcsign(otmp), 8),
               overheal_amount(u.uhpmax), !otmp->cursed, TRUE);
        make_hallucinated(0L, TRUE);
        exercise(A_CON, TRUE);
        exercise(A_STR, TRUE);
        break;
    case POT_FULL_HEALING:
        if (otmp->blessed && (u.ucramps > 1))
            u.ucramps = u.ucramps * 1 / 2 - 1;
        pline(msgc_statusheal, "You feel completely healed.");
        healup(250, 0, !otmp->cursed, TRUE);
        /* Restore one lost level if blessed */
        if (otmp->blessed && u.ulevel < u.ulevelmax) {
            /* when multiple levels have been lost, drinking multiple potions
               will only get half of them back, in challenge mode */
            if (challengemode)
                u.ulevelmax -= 1;
            pluslvl(FALSE);
        }
        make_hallucinated(0L, TRUE);
        exercise(A_STR, TRUE);
        exercise(A_CON, TRUE);
        break;
    case POT_LEVITATION:
    case SPE_LEVITATION:
        if (otmp->cursed)
            HLevitation &= ~I_SPECIAL;
        if (!Levitation) {
            /* kludge to ensure proper operation of float_up() */
            HLevitation = 1;
            float_up();
            /* reverse kludge */
            HLevitation = 0;
            if (otmp->cursed && !Is_waterlevel(&u.uz)) {
                if ((u.ux != level->upstair.sx || u.uy != level->upstair.sy) &&
                    (u.ux != level->sstairs.sx ||
                     u.uy != level->sstairs.sy || !level->sstairs.up) &&
                    (u.ux != level->upladder.sx ||
                     u.uy != level->upladder.sy)) {
                    pline(msgc_substitute, "You hit your %s on the %s.",
                          body_part(HEAD), ceiling(u.ux, u.uy));
                    losehp(uarmh ? 1 : rnd(10),
                           killer_msg(DIED, "colliding with the ceiling"));
                } else
                    doup();
            }
        } else
            nothing++;
        if (otmp->blessed) {
            incr_itimeout(&HLevitation, rn1(50, 250));
            HLevitation |= I_SPECIAL;
        } else
            incr_itimeout(&HLevitation, rn1(140, 10));
        spoteffects(FALSE);     /* for sinks */
        break;
    case POT_GAIN_ENERGY:      /* M. Stephenson */
        {
            int num;

            if (otmp->cursed)
                pline(msgc_substitute, "You feel lackluster.");
            else
                pline(msgc_statusheal,
                      "Magical energies course through your body.");
            num = rnd(3) + 3 * otmp->blessed + 1;
            u.uenmax += (otmp->cursed) ? -num : num;
            num = (u.uenmax <= 20) ? rnd(1 + u.uenmax * 2 / 3) :
                (abs(num) * rnd(1 + u.uenmax / 10));
            u.uen += (otmp->cursed) ? (1 + num / 2) : num;
            if (u.uenmax <= 0)
                u.uenmax = 0;
            if (u.uen <= 0)
                u.uen = 0;
            if (u.uen > u.uenmax)
                u.uen = u.uenmax;
        }
        break;
    case POT_OIL:      /* P. Winner */
        {
            boolean good_for_you = FALSE;

            if (otmp->lamplit) {
                if (likes_fire(youmonst.data)) {
                    pline(msgc_yafm, "Ahh, a refreshing drink.");
                    good_for_you = TRUE;
                } else {
                    pline(msgc_badidea, "You burn your %s.", body_part(FACE));
                    losehp(dice(Fire_resistance ? 1 : 3, 4),
                           killer_msg(DIED, "a burning potion of oil"));
                }
            } else if (otmp->cursed)
                pline(msgc_yafm, "This tastes like castor oil.");
            else
                pline(msgc_yafm, "That was smooth!");
            exercise(A_WIS, good_for_you);
        }
        break;
    case POT_ACID:
        if (Acid_resistance)
            /* Not necessarily a creature who _likes_ acid */
            pline(msgc_playerimmune, "This tastes %s.",
                  Hallucination ? "tangy" : "sour");
        else {
            pline(msgc_badidea, "This burns%s!", otmp->blessed ? " a little" :
                  otmp->cursed ? " a lot" : " like acid");
            losehp(dice(otmp->cursed ? 2 : 1, otmp->blessed ? 4 : 8),
                   killer_msg(DIED, "drinking acid"));
            exercise(A_CON, FALSE);
        }
        if (Stoned)
            fix_petrification();
        unkn++; /* holy/unholy water can burn like acid too */
        break;
    case POT_POLYMORPH:
        pline(Unchanging ? msgc_failcurse : msgc_actionok,
              "You feel a little %s.", Hallucination ? "normal" : "strange");
        if (!Unchanging)
            polyself(FALSE);
        break;
    default:
        impossible("What a funny potion! (%u)", otmp->otyp);
        return 0;
    }
    return -1;
}

void
healup(int nhp, int nxtra, boolean curesick, boolean cureblind)
{
    if (nhp) {
        if (Upolyd) {
            u.mh += nhp;
            if (u.mh > u.mhmax)
                u.mh = (u.mhmax += nxtra);
        } else {
            u.uhp += nhp;
            if (u.uhp > u.uhpmax)
                u.uhp = (u.uhpmax += nxtra);
        }
    }
    if (cureblind)
        make_blinded(0L, TRUE);
    if (curesick)
        make_sick(0L, NULL, TRUE, SICK_ALL);
    return;
}

void
strange_feeling(struct obj *obj, const char *txt)
{
    if (flags.beginner || !txt)
        pline(msgc_nospoil,
              "You have a %s feeling for a moment, then it passes.",
              Hallucination ? "normal" : "strange");
    else
        pline(msgc_hint, "%s", txt);

    if (!obj)   /* e.g., crystal ball finds no traps */
        return;

    if (obj->dknown && !objects[obj->otyp].oc_name_known &&
        !objects[obj->otyp].oc_uname)
        docall(obj);
    useup(obj);
}

static const char *const bottlenames[] = {
    "bottle", "phial", "flagon", "carafe", "flask", "jar", "vial"
};


const char *
bottlename(void)
{
    return bottlenames[rn2(SIZE(bottlenames))];
}

/* magr = potion thrower; &youmonst for you; NULL for unknown/not a monster */
void
potionhit(struct monst *mon, struct obj *obj, struct monst *magr)
{
    const char *botlnam = bottlename();
    boolean isyou = (mon == &youmonst);
    int distance;
    boolean lamplit;

    /* Remove any timers attached to the object (lit potion of oil). */
    lamplit = (obj->otyp == POT_OIL && obj->lamplit);
    if (obj->timed)
        obj_stop_timers(obj);

    if (isyou) {
        distance = 0;
        pline(combat_msgc(magr, mon, cr_hit),
              "The %s crashes on your %s and breaks into shards.", botlnam,
              body_part(HEAD));
        losehp(rnd(2), killer_msg(DIED, "a thrown potion"));
    } else {
        distance = distu(mon->mx, mon->my);
        if (!cansee(mon->mx, mon->my))
            pline(msgc_levelsound, "Crash!");
        else {
            const char *mnam = mon_nam(mon);
            const char *buf;

            if (has_head(mon->data))
                buf = msgprintf("%s %s", s_suffix(mnam),
                                (notonhead ? mbodypart(mon, BODY) : "head"));
            else
                buf = mnam;

            pline(combat_msgc(magr, mon, cr_hit),
                  "The %s crashes on %s and breaks into shards.", botlnam, buf);
        }
        if (rn2(5) && mon->mhp >= 2)
            mon->mhp--;
    }

    /* oil doesn't instantly evaporate */
    if (obj->otyp != POT_OIL && cansee(mon->mx, mon->my))
        pline_implied(msgc_consequence, "%s.", Tobjnam(obj, "evaporate"));

    if (isyou) {
        switch (obj->otyp) {
        case POT_OIL:
            if (lamplit)
                splatter_burning_oil(u.ux, u.uy);
            break;
        case POT_POLYMORPH:
            pline(combat_msgc(magr, mon, cr_hit), "You feel a little %s.",
                  Hallucination ? "normal" : "strange");
            if (!Unchanging && !Antimagic)
                polyself(FALSE);
            break;
        case POT_ACID:
            if (!Acid_resistance) {
                pline(combat_msgc(magr, mon, cr_hit), "This burns%s!",
                      obj->blessed ? " a little" : obj->cursed ? " a lot" : "");
                losehp(dice(obj->cursed ? 2 : 1, obj->blessed ? 4 : 8),
                       killer_msg(DIED, "being doused in acid"));
            }
            break;
        }
    } else {
        boolean angermon = magr == &youmonst;

        switch (obj->otyp) {
        case POT_HEALING:
        case POT_EXTRA_HEALING:
        case POT_FULL_HEALING:
            if (mon->data == &mons[PM_PESTILENCE])
                goto do_illness;
         /*FALLTHRU*/ case POT_RESTORE_ABILITY:
        case POT_GAIN_ABILITY:
        do_healing:
            angermon = FALSE;
            if (mon->mhp < mon->mhpmax) {
                mon->mhp = mon->mhpmax;
                if (canseemon(mon))
                    pline(magr == &youmonst ? msgc_actionok : msgc_monneutral,
                          "%s looks sound and hale again.", Monnam(mon));
            }
            break;
        case POT_SICKNESS:
            if (mon->data == &mons[PM_PESTILENCE])
                goto do_healing;
            if (dmgtype(mon->data, AD_DISE) ||
                dmgtype(mon->data, AD_PEST) || /* currently impossible */
                resists_poison(mon)) {
                if (canseemon(mon))
                    pline(combat_msgc(magr, mon, cr_immune),
                          "%s looks unharmed.", Monnam(mon));
                break;
            }
        do_illness:
            if ((mon->mhpmax > 3) && !resist(mon, POTION_CLASS, 0, NOTELL))
                mon->mhpmax /= 2;
            if ((mon->mhp > 2) && !resist(mon, POTION_CLASS, 0, NOTELL))
                mon->mhp /= 2;
            if (mon->mhp > mon->mhpmax)
                mon->mhp = mon->mhpmax;
            if (canseemon(mon))
                pline(combat_msgc(magr, mon, cr_hit),
                      "%s looks rather ill.", Monnam(mon));
            break;
        case POT_CONFUSION:
        case POT_BOOZE:
            if (!resist(mon, POTION_CLASS, 0, NOTELL))
                mon->mconf = TRUE;
            break;
        case POT_INVISIBILITY:
            angermon = FALSE;
            mon_set_minvis(mon);
            break;
        case POT_SLEEPING:
            /* wakeup() doesn't rouse victims of temporary sleep */
            if (sleep_monst(mon, rnd(12), POTION_CLASS)) {
                pline(combat_msgc(magr, mon, cr_hit),
                      "%s falls asleep.", Monnam(mon));
                slept_monst(mon);
            }
            break;
        case POT_PARALYSIS:
            if (mon->mcanmove) {
                mon->mcanmove = 0;
                /* really should be rnd(5) for consistency with players
                   breathing potions, but... */
                mon->mfrozen = rnd(25);
            }
            break;
        case POT_SPEED:
            angermon = FALSE;
            mon_adjust_speed(mon, 1, obj);
            break;
        case POT_BLINDNESS:
            if (haseyes(mon->data)) {
                int btmp = 64 + rn2(32) + rn2(32) *
                  !resist(mon, POTION_CLASS, 0, NOTELL);

                btmp += mon->mblinded;
                mon->mblinded = min(btmp, 127);
                mon->mcansee = 0;
            }
            break;
        case POT_WATER:
            if (is_undead(mon->data) || is_demon(mon->data) ||
                is_were(mon->data)) {
                if (obj->blessed) {
                    pline(combat_msgc(magr, mon, cr_hit),
                          "%s %s in pain!", Monnam(mon),
                          is_silent(mon->data) ? "writhes" : "shrieks");
                    if (!is_silent(mon->data))
                        aggravate();
                    mon->mhp -= dice(2, 6);
                    /* should only be by you */
                    if (mon->mhp <= 0)
                        killed(mon);
                    else if (is_were(mon->data) && !is_human(mon->data))
                        new_were(mon);  /* revert to human */
                } else if (obj->cursed) {
                    angermon = FALSE;
                    if (canseemon(mon))
                        pline(magr == &youmonst ?
                              msgc_actionok : msgc_monneutral,
                              "%s looks healthier.", Monnam(mon));
                    mon->mhp += dice(2, 6);
                    if (mon->mhp > mon->mhpmax)
                        mon->mhp = mon->mhpmax;
                    if (is_were(mon->data) && is_human(mon->data) &&
                        !Protection_from_shape_changers)
                        new_were(mon);  /* transform into beast */
                }
            } else if (mon->data == &mons[PM_GREMLIN]) {
                angermon = FALSE;
                split_mon(mon, NULL);
            } else if (mon->data == &mons[PM_IRON_GOLEM]) {
                if (canseemon(mon))
                    pline(combat_msgc(magr, mon, cr_hit), "%s rusts.",
                          Monnam(mon));
                mon->mhp -= dice(1, 6);
                /* should only be by you */
                if (mon->mhp <= 0)
                    killed(mon);
            }
            break;
        case POT_OIL:
            if (lamplit)
                splatter_burning_oil(mon->mx, mon->my);
            break;
        case POT_ACID:
            if (!resists_acid(mon) && !resist(mon, POTION_CLASS, 0, NOTELL)) {
                pline(combat_msgc(magr, mon, cr_hit), "%s %s in pain!",
                      Monnam(mon), is_silent(mon->data) ?
                      "writhes" : "shrieks");
                if (!is_silent(mon->data))
                    aggravate();
                mon->mhp -= dice(obj->cursed ? 2 : 1, obj->blessed ? 4 : 8);
                if (mon->mhp <= 0) {
                    if (magr == &youmonst)
                        killed(mon);
                    else
                        monkilled(magr, mon, "", AD_ACID);
                }
            }
            break;
        case POT_POLYMORPH:
            bhitm(&youmonst, mon, obj);
            break;
/*
        case POT_GAIN_LEVEL:
        case POT_LEVITATION:
        case POT_FRUIT_JUICE:
        case POT_MONSTER_DETECTION:
        case POT_OBJECT_DETECTION:
            break;
*/
        }
        if (angermon)
            wakeup(mon, FALSE);
        else
            mon->msleeping = 0;
    }

    /* Note: potionbreathe() does its own docall() */
    if ((distance == 0 || ((distance < 3) && rn2(5))) &&
        (!breathless(youmonst.data) || haseyes(youmonst.data)))
        potionbreathe(obj);
    else if (obj->dknown && !objects[obj->otyp].oc_name_known &&
             !objects[obj->otyp].oc_uname && cansee(mon->mx, mon->my))
        docall(obj);
    if (*u.ushops && obj->unpaid) {
        struct monst *shkp =
            shop_keeper(level, *in_rooms(level, u.ux, u.uy, SHOPBASE));

        if (!shkp)
            obj->unpaid = 0;
        else {
            stolen_value(obj, u.ux, u.uy, (boolean) shkp->mpeaceful, FALSE);
            subfrombill(obj, shkp);
        }
    }
    obfree(obj, NULL);
}


/* vapors are inhaled or get in your eyes */
void
potionbreathe(struct obj *obj)
{
    int i, ii, isdone, kn = 0;

    switch (obj->otyp) {
    case POT_RESTORE_ABILITY:
    case POT_GAIN_ABILITY:
        if (obj->cursed) {
            if (!breathless(youmonst.data))
                pline(msgc_noconsequence,
                      "Ulch!  That potion smells terrible!");
            else if (haseyes(youmonst.data)) {
                int numeyes = eyecount(youmonst.data);

                pline(msgc_noconsequence, "Your %s sting%s!",
                      (numeyes == 1) ? body_part(EYE) :
                      makeplural(body_part(EYE)),
                      (numeyes == 1) ? "s" : "");
            }
            break;
        } else {
            i = rn2(A_MAX);     /* start at a random point */
            for (isdone = ii = 0; !isdone && ii < A_MAX; ii++) {
                if (ABASE(i) < AMAX(i)) {
                    ABASE(i)++;
                    /* only first found if not blessed */
                    isdone = !(obj->blessed);
                }
                if (++i >= A_MAX)
                    i = 0;
            }
        }
        break;
    case POT_FULL_HEALING:
        if (Upolyd && u.mh < u.mhmax)
            u.mh++;
        if (u.uhp < u.uhpmax)
            u.uhp++;
        /* FALL THROUGH */
    case POT_EXTRA_HEALING:
        if (Upolyd && u.mh < u.mhmax)
            u.mh++;
        if (u.uhp < u.uhpmax)
            u.uhp++;
        /* FALL THROUGH */
    case POT_HEALING:
        if (Upolyd && u.mh < u.mhmax)
            u.mh++;
        if (u.uhp < u.uhpmax)
            u.uhp++;
        exercise(A_CON, TRUE);
        break;
    case POT_SICKNESS:
        if (!Role_if(PM_HEALER)) {
            if (Upolyd) {
                if (u.mh <= 5)
                    u.mh = 1;
                else
                    u.mh -= 5;
            } else {
                if (u.uhp <= 5)
                    u.uhp = 1;
                else
                    u.uhp -= 5;
            }
            exercise(A_CON, FALSE);
        }
        break;
    case POT_HALLUCINATION:
        kn++;
        pline(msgc_noconsequence, "You have a momentary vision.");
        break;
    case POT_CONFUSION:
    case POT_BOOZE:
        if (!Confusion)
            pline(msgc_statusbad, "You feel somewhat dizzy.");
        make_confused(itimeout_incr(HConfusion, rnd(5)), FALSE);
        break;
    case POT_INVISIBILITY:
        if (!Blind && !Invis) {
            kn++;
            pline(msgc_noconsequence, "For an instant you %s!",
                  See_invisible ? "could see right through yourself" :
                  "couldn't see yourself");
        }
        break;
    case POT_PARALYSIS:
        kn++;
        if (!Free_action) {
            pline(msgc_statusbad, "Something seems to be holding you.");
            helpless(5, hr_paralyzed, "frozen by potion vapours", NULL);
            exercise(A_DEX, FALSE);
        } else
            pline(msgc_playerimmune, "You stiffen momentarily.");
        break;
    case POT_SLEEPING:
        kn++;
        if (!Free_action && !Sleep_resistance) {
            pline(msgc_statusbad, "You feel rather tired.");
            helpless(5, hr_asleep, "sleeping off potion vapours", NULL);
            exercise(A_DEX, FALSE);
        } else
            pline(msgc_playerimmune, "You yawn.");
        break;
    case POT_SPEED:
        if (!Fast) {
            pline(msgc_statusgood, "Your knees seem more flexible now.");
            kn++;
        }
        incr_itimeout(&HFast, rnd(5));
        exercise(A_DEX, TRUE);
        break;
    case POT_BLINDNESS:
        if (!Blind) {
            kn++;
            pline(msgc_statusbad, "It suddenly gets dark.");
        }
        make_blinded(itimeout_incr(Blinded, rnd(5)), FALSE);
        if (!Blind)
            pline(msgc_statusheal, "Your vision quickly clears.");
        break;
    case POT_WATER:
        if (u.umonnum == PM_GREMLIN) {
            split_mon(&youmonst, NULL);
        } else if (u.ulycn >= LOW_PM) {
            /* vapor from [un]holy water will trigger transformation but won't
               cure lycanthropy */
            if (obj->blessed && youmonst.data == &mons[u.ulycn])
                you_unwere(FALSE);
            else if (obj->cursed && !Upolyd)
                you_were();
        }
        break;
    case POT_ACID:
        /* Acid damage handled elsewhere. */
        if (!Acid_resistance)
            kn++;
        exercise(A_CON, FALSE);
        break;
    case POT_POLYMORPH:
        /* Potion of polymorph always gives a unique message, even if
           unchanging. */
        kn++;
        exercise(A_CON, FALSE);
        break;
/*
    case POT_GAIN_LEVEL:
    case POT_LEVITATION:
    case POT_FRUIT_JUICE:
    case POT_MONSTER_DETECTION:
    case POT_OBJECT_DETECTION:
    case POT_OIL:
        break;
*/
    }
    /* note: no obfree() */
    if (obj->dknown) {
        if (kn)
            makeknown(obj->otyp);
        else if (!objects[obj->otyp].oc_name_known &&
                 !objects[obj->otyp].oc_uname)
            docall(obj);
    }
}


static short
mixtype(struct obj *o1, struct obj *o2)
/* returns the potion type when o1 is dipped in o2 */
{
    /* cut down on the number of cases below */
    if (o1->oclass == POTION_CLASS &&
        (o2->otyp == POT_GAIN_LEVEL || o2->otyp == POT_GAIN_ENERGY ||
         o2->otyp == POT_HEALING || o2->otyp == POT_EXTRA_HEALING ||
         o2->otyp == POT_FULL_HEALING || o2->otyp == POT_ENLIGHTENMENT ||
         o2->otyp == POT_FRUIT_JUICE)) {
        struct obj *swp;

        swp = o1;
        o1 = o2;
        o2 = swp;
    }

    switch (o1->otyp) {
    case POT_HEALING:
        switch (o2->otyp) {
        case POT_SPEED:
            return POT_FULL_HEALING;
        case POT_GAIN_LEVEL:
        case POT_GAIN_ENERGY:
            return POT_EXTRA_HEALING;
        }
    case POT_EXTRA_HEALING:
        switch (o2->otyp) {
        case POT_GAIN_LEVEL:
        case POT_GAIN_ENERGY:
            return POT_FULL_HEALING;
        }
    case POT_FULL_HEALING:
        switch (o2->otyp) {
        case POT_GAIN_LEVEL:
            return POT_GAIN_ENERGY;
        case POT_GAIN_ENERGY:
            return POT_GAIN_ABILITY;
        }
    case UNICORN_HORN:
        switch (o2->otyp) {
        case POT_SICKNESS:
            return POT_FRUIT_JUICE;
        case POT_HALLUCINATION:
        case POT_BLINDNESS:
        case POT_CONFUSION:
            return POT_WATER;
        }
        break;
    case AMETHYST:     /* "a-methyst" == "not intoxicated" */
        if (o2->otyp == POT_BOOZE) {
            makeknown(AMETHYST);
            makeknown(POT_BOOZE);
            makeknown(POT_FRUIT_JUICE);
            return POT_FRUIT_JUICE;
        }
        break;
    case POT_GAIN_LEVEL:
        if (o2->otyp == POT_FULL_HEALING)
            return POT_GAIN_ENERGY;
        /* else fall through */
    case POT_GAIN_ENERGY:
        switch (o2->otyp) {
        case POT_CONFUSION:
            return rn2(3) ? POT_BOOZE : POT_ENLIGHTENMENT;
        case POT_HEALING:
            return POT_EXTRA_HEALING;
        case POT_EXTRA_HEALING:
            return POT_FULL_HEALING;
        case POT_FULL_HEALING:
            return POT_GAIN_ABILITY;
        case POT_FRUIT_JUICE:
            return POT_SIGHT;
        case POT_BOOZE:
            return POT_HALLUCINATION;
        }
        break;
    case POT_FRUIT_JUICE:
        switch (o2->otyp) {
        case POT_SICKNESS:
            return POT_SICKNESS;
        case POT_SPEED:
            return POT_BOOZE;
        case POT_GAIN_LEVEL:
        case POT_GAIN_ENERGY:
            return POT_SIGHT;
        }
        break;
    case POT_ENLIGHTENMENT:
        switch (o2->otyp) {
        case POT_LEVITATION:
            if (rn2(3))
                return POT_GAIN_LEVEL;
            break;
        case POT_FRUIT_JUICE:
            return POT_BOOZE;
        case POT_BOOZE:
            return POT_CONFUSION;
        }
        break;
    }

    return 0;
}


int
dodip(const struct nh_cmd_arg *arg)
{
    struct obj *obj, *potion;
    struct obj *singlepotion;
    boolean allowfloor = FALSE;
    const char *tmp;
    uchar here;
    char allowall[2] = { ALL_CLASSES, 0 };
    short mixture;
    const char *qbuf, *Your_buf = NULL;

    /* The dip /into/ is taken from arg; the object to dip must therefore
       be propmted for each time.

       TODO: "What do you want to dip into your potions of sickness?" */
    obj = getobj(allowall, "dip", FALSE);
    if (!obj)
        return 0;

    here = level->locations[u.ux][u.uy].typ;
    /* Is there a fountain or pool to dip into here? */
    if ((IS_FOUNTAIN(here) || is_damp_terrain(level, u.ux, u.uy)) &&
        !Levitation &&
        !(u.usteed && !is_swimmer(u.usteed->data) &&
          P_SKILL(P_RIDING) < P_BASIC &&
          !IS_FOUNTAIN(here)))
        allowfloor = TRUE;


    qbuf = msgprintf("dip %s into",
                     safe_qbuf("", sizeof ("dip  into"), the(xname(obj)),
                               the(simple_typename(obj->otyp)), "this item"));
    potion = getargobj(arg, allowfloor ? beverages_and_fountains : beverages,
                       qbuf);
    if (!potion)
        return 0;

    if (potion == &zeroobj) {
        if (IS_FOUNTAIN(here))
            dipfountain(obj);
        else {
            water_damage(obj, NULL, TRUE);
            if (obj->otyp == POT_ACID)
                useup(obj);
        }
        return 1;
    }

    if (potion == obj && potion->quan == 1L) {
        pline(msgc_cancelled, "That is a potion bottle, not a Klein bottle!");
        return 0;
    }
    potion->in_use = TRUE;      /* assume it will be used up */
    if (potion->otyp == POT_WATER) {
        boolean useeit = !Blind;
        boolean usedup = FALSE;

        if (useeit)
            Your_buf = Shk_Your(obj);
        if (potion->blessed) {
            if (obj->cursed) {
                if (useeit)
                    pline(msgc_itemrepair, "%s %s %s.", Your_buf,
                          aobjnam(obj, "softly glow"), hcolor("amber"));
                uncurse(obj);
                obj->bknown = 1;
                usedup = TRUE;
            } else if (!obj->blessed) {
                if (useeit) {
                    tmp = hcolor("light blue");
                    pline(msgc_itemrepair, "%s %s with a%s %s aura.",
                          Your_buf, aobjnam(obj, "softly glow"),
                          strchr(vowels, *tmp) ? "n" : "", tmp);
                }
                bless(obj);
                obj->bknown = 1;
                usedup = TRUE;
            }
        } else if (potion->cursed) {
            if (obj->blessed) {
                if (useeit)
                    pline(msgc_itemloss, "%s %s %s.", Your_buf,
                          aobjnam(obj, "glow"), hcolor((const char *)"brown"));
                unbless(obj);
                obj->bknown = 1;
                usedup = TRUE;
            } else if (!obj->cursed) {
                if (useeit) {
                    tmp = hcolor("black");
                    pline(msgc_itemloss, "%s %s with a%s %s aura.", Your_buf,
                          aobjnam(obj, "glow"), strchr(vowels, *tmp) ? "n" : "",
                          tmp);
                }
                curse(obj);
                obj->bknown = 1;
                usedup = TRUE;
            }
        } else if (water_damage(obj, NULL, TRUE) >= 2)
            usedup = TRUE;

        if (usedup) {
            makeknown(POT_WATER);
            useup(potion);
            return 1;
        }

    } else if (potion->otyp == POT_POLYMORPH) {
        /* some objects can't be polymorphed */
        if (poly_proof(obj) || obj_resists(obj, 5, 95)) {
            pline(msgc_failrandom, "Nothing happens.");
        } else {
            int save_otyp = obj->otyp;

            /* KMH, conduct */
            break_conduct(conduct_polypile);

            obj = poly_obj(obj, STRANGE_OBJECT);

            if (!obj || obj->otyp != save_otyp) {
                makeknown(POT_POLYMORPH);
                useup(potion);
                if (obj && !(obj->owornmask & W_EQUIP))
                    /* If equipped, this will already have been done. */
                    prinv(NULL, obj, 0L);
                return 1;
            } else {
                /* different message from 3.4.3; having this almost identical
                   to the regular message is silly, given that the message is
                   unique and the fact that the potion is used up is a
                   giveaway that something is up */
                pline(msgc_failrandom,
                      "The object you dipped changed slightly.");
                makeknown(POT_POLYMORPH);
                useup(potion);
                return 1;
            }
        }
        potion->in_use = FALSE; /* didn't go poof */
        return 1;
    } else if (obj->oclass == POTION_CLASS && obj->otyp != potion->otyp) {
        /* Mixing potions is dangerous... */
        pline(msgc_occstart, "The potions mix...");
        /* KMH, balance patch -- acid is particularly unstable */
        /* AIS: We use a custom RNG for this; many players alchemize only once
           or twice per game with an enormous stack of potions, and whether they
           survive or are consumed has balance implications */
        if (obj->cursed || obj->otyp == POT_ACID ||
            !rn2_on_rng(10, rng_alchemic_blast)) {
            pline(msgc_failrandom, "BOOM!  They explode!");
            wake_nearby(FALSE);
            exercise(A_STR, FALSE);
            if (!breathless(youmonst.data) || haseyes(youmonst.data))
                potionbreathe(obj);
            useup(obj);
            useup(potion);
            losehp(rnd(10), killer_msg(DIED, "an alchemic blast"));
            return 1;
        }

        obj->blessed = obj->cursed = obj->bknown = 0;
        if (Blind || Hallucination)
            obj->dknown = 0;

        if ((mixture = mixtype(obj, potion)) != 0) {
            obj->otyp = mixture;
        } else {
            switch (obj->odiluted ? 1 : rnd(8)) {
            case 1:
                obj->otyp = POT_WATER;
                break;
            case 2:
            case 3:
                obj->otyp = POT_SICKNESS;
                break;
            case 4:
                {
                    struct obj *otmp;

                    otmp = mkobj(level, POTION_CLASS, FALSE, rng_main);
                    obj->otyp = otmp->otyp;
                    obfree(otmp, NULL);
                }
                break;
            default:
                if (!Blind)
                    pline(msgc_failrandom,
                          "The mixture glows brightly and evaporates.");
                useup(obj);
                useup(potion);
                return 1;
            }
        }

        obj->odiluted = (obj->otyp != POT_WATER);

        if (obj->otyp == POT_WATER && !Hallucination) {
            pline(msgc_failrandom, "The mixture bubbles%s.",
                  Blind ? "" : ", then clears");
        } else if (!Blind) {
            pline(msgc_actionok, "The mixture looks %s.",
                  hcolor(OBJ_DESCR(objects[obj->otyp])));
        }

        useup(potion);
        return 1;
    }
#ifdef INVISIBLE_OBJECTS
    if (potion->otyp == POT_INVISIBILITY && !obj->oinvis) {
        obj->oinvis = TRUE;
        if (!Blind) {
            if (!See_invisible)
                pline(msgc_actionok, "Where did %s go?", the(xname(obj)));
            else
                pline(msgc_actionok, "You notice a little haziness around %s.",
                      the(xname(obj)));
        }
        makeknown(POT_INVISIBILITY);
        useup(potion);
        return 1;
    } else if (potion->otyp == POT_SIGHT && obj->oinvis) {
        obj->oinvis = FALSE;
        if (!Blind) {
            if (!See_invisible)
                pline(msgc_actionok, "So that's where %s went!",
                      the(xname(obj)));
            else
                pline(msgc_actionok, "The haziness around %s disappears.",
                      the(xname(obj)));
        }
        makeknown(POT_SIGHT);
        useup(potion);
        return 1;
    }
#endif

    if (is_poisonable(obj)) {
        if (potion->otyp == POT_SICKNESS && !obj->opoisoned) {
            const char *buf;

            if (potion->quan > 1L)
                buf = msgcat("One of ", the(xname(potion)));
            else
                buf = The(xname(potion));
            pline(msgc_itemrepair, "%s forms a coating on %s.", buf,
                  the(xname(obj)));
            obj->opoisoned = TRUE;
            makeknown(POT_SICKNESS);
            useup(potion);
            return 1;
        } else if (obj->opoisoned &&
                   (potion->otyp == POT_HEALING ||
                    potion->otyp == POT_EXTRA_HEALING ||
                    potion->otyp == POT_FULL_HEALING)) {
            pline(msgc_itemrepair, "A coating wears off %s.", the(xname(obj)));
            obj->opoisoned = 0;
            /* trigger the "recently broken" prompt because there are
               multiple possibilities */
            if (!(objects[potion->otyp].oc_name_known) &&
                !(objects[potion->otyp].oc_uname))
                docall(potion);
            useup(potion);
            return 1;
        }
    }

    if (potion->otyp == POT_OIL) {

        if (potion->lamplit) {  /* burning */
            int omat = objects[obj->otyp].oc_material;

            /* the code here should be merged with fire_damage */
            if (catch_lit(obj)) {
                /* catch_lit does all the work if true */
            } else if (obj->oerodeproof || obj_resists(obj, 5, 95) ||
                       !is_flammable(obj) || obj->oclass == FOOD_CLASS) {
                pline(msgc_failrandom, "%s %s to burn for a moment.",
                      Yname2(obj), otense(obj, "seem"));
            } else {
                if ((omat == PLASTIC || omat == PAPER) && !obj->oartifact)
                    obj->oeroded = MAX_ERODE;
                pline(msgc_itemloss, "The burning oil %s %s.",
                      obj->oeroded == MAX_ERODE ? "destroys" : "damages",
                      yname(obj));
                if (obj->oeroded == MAX_ERODE) {
                    obj_extract_self(obj);
                    obfree(obj, NULL);
                    obj = NULL;
                } else {
                    /* we know it's carried */
                    if (obj->unpaid) {
                        /* create a dummy duplicate to put on bill */
                        if (Deaf)
                            pline(msgc_unpaid,
                                  "The damage is placed on your bill.");
                        else
                            verbalize(msgc_unpaid,
                                      "You burnt it, you bought it!");
                        bill_dummy_object(obj);
                    }
                    obj->oeroded++;
                }
            }
        } else if (Is_candle(obj) && obj->lamplit) {
            pline(msgc_consequence, "The oil catches the flame.");
            (void) light_cocktail(potion);
            potion->in_use = FALSE;
            return 0;
        } else if (potion->cursed) {
            pline(msgc_failcurse,
                  "The potion spills and covers your %s with oil.",
                  makeplural(body_part(FINGER)));
            incr_itimeout(&Glib, dice(2, 10));
        } else if (obj->oclass != WEAPON_CLASS && !is_weptool(obj)) {
            /* the following cases apply only to weapons */
            goto more_dips;
            /* Oil removes rust and corrosion, but doesn't unburn. Arrows, etc
               are classed as metallic due to arrowhead material, but dipping
               in oil shouldn't repair them. */
        } else if ((!is_rustprone(obj) && !is_corrodeable(obj)) || is_ammo(obj)
                   || (!obj->oeroded && !obj->oeroded2)) {
            /* uses up potion, doesn't set obj->greased */
            pline(msgc_yafm, "%s %s with an oily sheen.", Yname2(obj),
                  otense(obj, "gleam"));
        } else {
            pline(msgc_itemrepair, "%s %s less %s.", Yname2(obj),
                  otense(obj, "are"), (obj->oeroded && obj->oeroded2) ?
                  "corroded and rusty" : obj->oeroded ? "rusty" : "corroded");
            if (obj->oeroded > 0)
                obj->oeroded--;
            if (obj->oeroded2 > 0)
                obj->oeroded2--;
        }
        makeknown(potion->otyp);
        useup(potion);
        return 1;
    }
more_dips:

    /* Allow filling of MAGIC_LAMPs to prevent identification by player */
    if ((obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP) &&
        (potion->otyp == POT_OIL)) {
        /* Turn off engine before fueling, turn off fuel too :-) */
        if (obj->lamplit || potion->lamplit) {
            useup(potion);
            explode(u.ux, u.uy, 11, dice(6, 6), 0, EXPL_FIERY, NULL, 0);
            return 1;
        }
        /* Adding oil to an empty magic lamp renders it into an oil lamp */
        if ((obj->otyp == MAGIC_LAMP) && obj->spe == 0) {
            obj->otyp = OIL_LAMP;
            obj->age = 0;
        }
        if (obj->age > 1000L) {
            pline(msgc_yafm, "%s %s full.", Yname2(obj), otense(obj, "are"));
            potion->in_use = FALSE;     /* didn't go poof */
        } else {
            pline(msgc_actionok, "You fill %s with oil.", yname(obj));
            check_unpaid(potion);       /* Yendorian Fuel Tax */
            obj->age += 2 * potion->age;        /* burns more efficiently */
            if (obj->age > 1500L)
                obj->age = 1500L;
            useup(potion);
        }
        makeknown(POT_OIL);
        obj->spe = 1;
        update_inventory();
        return 1;
    }

    potion->in_use = FALSE;     /* didn't go poof */
    if ((obj->otyp == UNICORN_HORN || obj->otyp == AMETHYST) &&
        (mixture = mixtype(obj, potion)) != 0) {
        short old_otyp = potion->otyp;
        boolean old_dknown = FALSE;
        boolean more_than_one = potion->quan > 1;
        const char *oldbuf, *newbuf;

        oldbuf = "";
        if (potion->dknown) {
            old_dknown = TRUE;
            oldbuf = msgcat(hcolor(OBJ_DESCR(objects[potion->otyp])), " ");
        }
        /* with multiple merged potions, split off one and just clear it */
        if (potion->quan > 1L) {
            singlepotion = splitobj(potion, 1L);
        } else
            singlepotion = potion;

        if (singlepotion->unpaid && costly_spot(u.ux, u.uy)) {
            pline(msgc_unpaid, "You use it, you pay for it.");
            bill_dummy_object(singlepotion);
        }
        singlepotion->otyp = mixture;
        singlepotion->blessed = 0;
        if (mixture == POT_WATER)
            singlepotion->cursed = singlepotion->odiluted = 0;
        else
            singlepotion->cursed = obj->cursed; /* odiluted left as-is */
        singlepotion->bknown = FALSE;
        if (Blind) {
            singlepotion->dknown = FALSE;
        } else {
            singlepotion->dknown = !Hallucination;
            if (mixture == POT_WATER && singlepotion->dknown)
                newbuf = "clears";
            else
                newbuf = msgcat("turns ", hcolor(OBJ_DESCR(objects[mixture])));
            pline(msgc_actionok, "The %spotion%s %s.", oldbuf,
                  more_than_one ? " that you dipped into" : "", newbuf);
            if (!objects[old_otyp].oc_uname && !objects[old_otyp].oc_name_known
                && old_dknown) {
                struct obj *fakeobj = mktemp_sobj(NULL, old_otyp);

                fakeobj->dknown = 1;
                docall(fakeobj);
                obfree(fakeobj, NULL);
            }
        }
        obj_extract_self(singlepotion);
        hold_another_object(singlepotion, "You juggle and drop %s!",
                            doname(singlepotion), NULL);
        update_inventory();
        return 1;
    }

    pline(msgc_cancelled1, "Interesting...");
    return 1;
}


void
djinni_from_bottle(struct obj *obj)
{
    struct monst *mtmp;

    if (!(mtmp = makemon(&mons[PM_DJINNI], level, u.ux, u.uy, NO_MM_FLAGS))) {
        pline(msgc_noconsequence, "It turns out to be empty.");
        return;
    }

    if (!Blind) {
        pline_implied(msgc_occstart, "In a cloud of smoke, %s emerges!",
              a_monnam(mtmp));
        pline_implied(msgc_npcvoice, "%s speaks%s.", Monnam(mtmp),
                      Deaf ? msgprintf(", but you cannot hear%s",
                                       mhim(mtmp)) : "");
    } else {
        pline_implied(msgc_occstart, "You smell acrid fumes.");
        if (!Deaf)
            pline_implied(msgc_npcvoice, "Something speaks.");
    }

    int dieroll;
    /* The wish has the largest balance implications; use wish_available to
       balance it against other wish sources with the same probability; also
       find out what the dieroll was so that the non-wish results will be
       consistent if the only 80%/20%/5% wish sources so far were djinn */
    if (wish_available(obj->blessed ? 80 : obj->cursed ? 5 : 20, &dieroll)) {
        msethostility(mtmp, FALSE, TRUE); /* show as peaceful while wishing */
        if (!Deaf)
            verbalize(msgc_intrgain,
                      "I am in your debt.  I will grant one wish!");
        else if (!Blind)
            pline_implied(msgc_intrgain, "%s seems grateful!",
                          Monnam(mtmp));
        /* Even if you can't hear or see the genie, you still get the wish,
           because thinking up Evil Patch Ideas is one thing, and actually
           implementing them is something else.  How does the character know he
           can wish for something in that case?  We're going to wave our hands
           and mutter something vague about "magic" and let it go at that.
           After all, what player would complain about getting a wish? */
        makewish(1);
        mongone(mtmp);
        return;
    }

    /* otherwise, typically an equal chance of each other result, although
       for a cursed potion, we have a hostile djinni with an 80% chance */
    if (obj->cursed && dieroll >= 20)
        dieroll = 0;

    /* avoid failrandom here; that may have been spammed beforehand */
    switch (dieroll % 4) {
    case 0:
        if (!Deaf)
            verbalize(msgc_substitute, "You disturbed me, fool!");
        else if (!Blind)
            pline(msgc_substitute, "%s seems upset!",
                  msgupcasefirst(mhe(mtmp)));
        msethostility(mtmp, TRUE, TRUE);
        break;
    case 1:
        if (!Deaf)
            verbalize(msgc_actionok, "Thank you for freeing me!");
        else if (!Blind)
            pline(msgc_actionok, "%s seems quite pleased.",
                  msgupcasefirst(mhe(mtmp)));
        tamedog(mtmp, NULL);
        break;
    case 2:
        if (!Deaf)
            verbalize(msgc_actionok, "You freed me!");
        else if (!Blind)
            pline(msgc_actionok, "%s seems pleased.",
                  msgupcasefirst(mhe(mtmp)));
        msethostility(mtmp, FALSE, TRUE);
        break;
    case 3:
        if (!Deaf)
            verbalize(msgc_actionok, "It is about time!");
        if (canseemon(mtmp))
            pline(msgc_monneutral, "%s vanishes.", Monnam(mtmp));
        mongone(mtmp);
        break;
    }
}

/* clone a gremlin or mold (2nd arg non-null implies heat as the trigger);
   hit points are cut in half (odd HP stays with original) */
struct monst *
split_mon(struct monst *mon,    /* monster being split */
          struct monst *mtmp)
{       /* optional attacker whose heat triggered it */
    struct monst *mtmp2;
    const char *reason;

    reason = "";
    if (mtmp)
        reason = msgprintf(" from %s heat", (mtmp == &youmonst) ?
                           (const char *)"your" :
                           (const char *)s_suffix(mon_nam(mtmp)));

    if (mon == &youmonst) {
        mtmp2 = cloneu();
        if (mtmp2) {
            mtmp2->mhpmax = u.mhmax / 2;
            u.mhmax -= mtmp2->mhpmax;
            pline(msgc_statusgood, "You multiply%s!", reason);
        }
    } else {
        mtmp2 = clone_mon(mon, 0, 0);
        if (mtmp2) {
            mtmp2->mhpmax = mon->mhpmax / 2;
            mon->mhpmax -= mtmp2->mhpmax;
            if (mon->msplitcount + 1 < 65535)
                mon->msplitcount++;
            mtmp2->msplitcount = mon->msplitcount;
            if (canspotmon(mon))
                pline(combat_msgc(mon, NULL, cr_hit),
                      "%s multiplies%s!", Monnam(mon), reason);
        }
    }
    return mtmp2;
}

/*potion.c*/
