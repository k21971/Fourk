/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2015-07-20 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

static const char *const hu_stat[] = {
    "Bloated",
    NULL,
    "Hungry",
    "Weak",
    "Fainting",
    "Fainted",
    "Starved"
};

static const char *const enc_stat[] = {
    NULL,
    "Burdened",
    "Stressed",
    "Strained",
    "Overtaxed",
    "Overloaded"
};

static const char *const trap_stat[] = {
    "Beartrap",
    "Pit",
    "Web",
    "Lava",
    "Infloor",
    "IceB",
};


static int mrank_sz = 0;        /* loaded by max_rank_sz (from u_init) */

static const char *rank(void);
static long botl_score(void);

/* Historically, this returned a named rank, such as "Preegrinatrix" or
   "Man-at-arms", which were deterministic based on level and role but both the
   assignment of ranks to roles, and the ordering of the ranks for each role,
   were so arbitrary and meaningless that nobody could remember them.  Also,
   the rank titles added nothing at all to the game.
*/
const char *
rank_of(int lev, short monnum, boolean female)
{
    const struct Role *role;

    /* Find the role */
    for (role = roles; role->name.m; role++)
        if (monnum == role->malenum || monnum == role->femalenum)
            break;
    if (!role->name.m)
        role = &urole;

    if (female && role->name.f)
        return role->name.f;
    else if (role->name.m)
        return role->name.m;
    return "Player";
}


static const char *
rank(void)
{
    return rank_of(u.ulevel, Role_switch, u.ufemale);
}

void
max_rank_sz(void)
{
    int r, maxr = strlen(urole.name.m);
    r = urole.name.f ? strlen(urole.name.f) : 0;
    if (r > maxr) maxr = r;
    mrank_sz = maxr;
    return;
}


static long
botl_score(void)
{
    long umoney = money_cnt(invent) + hidden_gold();

    return calc_score(DIED, FALSE, umoney);
}


/* provide the name of the current level */
int
describe_level(char *buf)
{
    int ret = 1;

    if (Is_knox(&u.uz))
        sprintf(buf, "%s", find_dungeon(&u.uz).dname);
    else if (In_quest(&u.uz))
        sprintf(buf, "Home:%d", dunlev(&u.uz));
    else if (In_endgame(&u.uz))
        sprintf(buf, Is_astralevel(&u.uz) ? "Astral Plane" : "End Game");
    else if (In_mines(&u.uz))
        sprintf(buf, "Mines:%d", depth(&u.uz));
    else if (In_sokoban(&u.uz))
        sprintf(buf, "Sokoban:%d", depth(&u.uz));
    else if (Is_valley(&u.uz))
        sprintf(buf, "Valley:%d", depth(&u.uz));
    else if (In_hell(&u.uz))
        sprintf(buf, "Gehennom:%d", depth(&u.uz));
    else if (In_V_tower(&u.uz))
        sprintf(buf, "Tower:%d", depth(&u.uz));
    else
        sprintf(buf, "Dungeons:%d", depth(&u.uz)), (ret = 0);
    return ret;
}

static void
make_player_info(struct nh_player_info *pi)
{
    int cap, advskills, i;

    memset(pi, 0, sizeof (struct nh_player_info));

    pi->moves = moves;
    pi->speed = you_speed(TRUE);
    strncpy(pi->plname, u.uplname, sizeof (pi->plname));
    pi->align = u.ualign.type;

    /* This function could be called before the game is fully inited. Test
       youmonst.data as it is required for near_capacity().
       program_state.game_running is no good, as we need this data before
       game_running is set.

       TODO: Wow this is hacky. */
    if (!youmonst.data)
        return;
    API_ENTRY_CHECKPOINT_RETURN_VOID_ON_ERROR();

    pi->x = u.ux;
    pi->y = u.uy;
    pi->z = u.uz.dlevel;

    if (Upolyd) {
        strncpy(pi->rank, msgtitlecase(mons[u.umonnum].mname),
                sizeof (pi->rank));
    } else
        strncpy(pi->rank, rank(), sizeof (pi->rank));

    strncpy(pi->rolename, (u.ufemale && urole.name.f) ?
            urole.name.f : urole.name.m, sizeof (pi->rolename));
    strncpy(pi->racename, urace.noun, sizeof (pi->racename));
    strncpy(pi->gendername, genders[u.ufemale].adj,
            sizeof(pi->gendername));

    pi->max_rank_sz = mrank_sz;

    /* abilities */
    pi->st = ACURR(A_STR);
    pi->st_extra = 0;
    if (pi->st > 118) {
        pi->st = pi->st - 100;
        pi->st_extra = 0;
    } else if (pi->st > 18) {
        pi->st_extra = pi->st - 18;
        pi->st = 18;
    }

    pi->dx = ACURR(A_DEX);
    pi->co = ACURR(A_CON);
    pi->in = ACURR(A_INT);
    pi->wi = ACURR(A_WIS);
    pi->ch = ACURR(A_CHA);

    pi->score = botl_score();

    /* hp and energy */
    pi->hp = Upolyd ? u.mh : u.uhp;
    pi->hpmax = Upolyd ? u.mhmax : u.uhpmax;
    if (pi->hp < 0)
        pi->hp = 0;

    pi->en = u.uen;
    pi->enmax = u.uenmax;
    pi->ac = get_player_ac();

    pi->gold = money_cnt(invent);
    pi->coinsym = def_oc_syms[COIN_CLASS];
    describe_level(pi->level_desc);

    pi->monnum = u.umonster;
    pi->cur_monnum = u.umonnum;

    /* level and exp points */
    if (Upolyd)
        pi->level = mons[u.umonnum].mlevel;
    else
        pi->level = u.ulevel;
    pi->xp = u.uexp;

    cap = near_capacity();

    /* check if any skills could be anhanced */
    advskills = 0;
    for (i = 0; i < P_NUM_SKILLS; i++) {
        if (P_RESTRICTED(i))
            continue;
        if (can_advance(i, FALSE))
            advskills++;
    }
    pi->can_enhance = advskills > 0;

    /* add status items for various problems there can be at most 24 items here 
       at any one time or we overflow the buffer */
    if (hu_stat[u.uhs]) /* 1 */
        strncpy(pi->statusitems[pi->nr_items++], hu_stat[u.uhs], ITEMLEN);

    if (Confusion)      /* 2 */
        strncpy(pi->statusitems[pi->nr_items++], "Conf", ITEMLEN);

    if (Sick) { /* 3 */
        if (u.usick_type & SICK_VOMITABLE)
            strncpy(pi->statusitems[pi->nr_items++], "FoodPois", ITEMLEN);
        if (u.usick_type & SICK_NONVOMITABLE)
            strncpy(pi->statusitems[pi->nr_items++], "Ill", ITEMLEN);
    }
    if (Blind)  /* 4 */
        strncpy(pi->statusitems[pi->nr_items++], "Blind", ITEMLEN);
    if (Glib)   /* 5 */
        strncpy(pi->statusitems[pi->nr_items++], "Greasy", ITEMLEN);
    if (Wounded_legs)   /* 6 */
        strncpy(pi->statusitems[pi->nr_items++], "Lame", ITEMLEN);
    if (Stunned)        /* 7 */
        strncpy(pi->statusitems[pi->nr_items++], "Stun", ITEMLEN);
    if (Hallucination)  /* 8 */
        strncpy(pi->statusitems[pi->nr_items++], "Hallu", ITEMLEN);
    if (Strangled)      /* 9 */
        strncpy(pi->statusitems[pi->nr_items++], "Strangle", ITEMLEN);
    if (Slimed) /* 10 */
        strncpy(pi->statusitems[pi->nr_items++], "Slime", ITEMLEN);
    if (Stoned) /* 11 */
        strncpy(pi->statusitems[pi->nr_items++], "Petrify", ITEMLEN);
    if (u.ustuck && !Engulfed && !sticks(youmonst.data))      /* 12 */
        strncpy(pi->statusitems[pi->nr_items++], "Held", ITEMLEN);
    if (enc_stat[cap])  /* 13, encumbrance level */
        strncpy(pi->statusitems[pi->nr_items++], enc_stat[cap], ITEMLEN);
    if (Deaf) /* 14 */
        strncpy(pi->statusitems[pi->nr_items++], "Deaf", ITEMLEN);
    if (Levitation)     /* 15 */
        strncpy(pi->statusitems[pi->nr_items++], "Lev", ITEMLEN);
    else if (Flying)
        strncpy(pi->statusitems[pi->nr_items++], "Fly", ITEMLEN);
    if (uwep && is_pick(uwep)) /* 16 (first case) */
        strncpy(pi->statusitems[pi->nr_items++], "Dig", ITEMLEN);
    else if (uwep && is_pole(uwep))
        strncpy(pi->statusitems[pi->nr_items++], "Pole", ITEMLEN);
    else if (uwep && is_launcher(uwep))
        strncpy(pi->statusitems[pi->nr_items++], "Ranged", ITEMLEN);
    else if (uwep && (uwep->otyp == CORPSE) && (touch_petrifies(&mons[uwep->corpsenm])))
        strncpy(pi->statusitems[pi->nr_items++], "Trice", ITEMLEN);
    else if (!uwep)
        strncpy(pi->statusitems[pi->nr_items++], "Unarmed", ITEMLEN);
    else if (!is_wep(uwep))
        strncpy(pi->statusitems[pi->nr_items++], "NonWeap", ITEMLEN);
    else {
        /* strncpy(pi->statusitems[pi->nr_items++], "Melee", ITEMLEN); */
        /* Don't show the default Melee status light, as that's the most common case. */
        /* 15 (last case) */
    }
    if (youmonst.mslowed > 0)
        strncpy(pi->statusitems[pi->nr_items++], "Slow", ITEMLEN);
    if (u.utrap)        /* 17 */
        strncpy(pi->statusitems[pi->nr_items++], trap_stat[u.utraptype],
                ITEMLEN);

    API_EXIT();
}


void
bot(void)
{
    struct nh_player_info pi;

    make_player_info(&pi);
    update_status(&pi);
}

/*botl.c*/

