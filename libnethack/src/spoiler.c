/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2017-10-07 */
/* File opening code based on the xlogfile code. */
/* Object-looping code based on makedefs.c */
/* Concept based on Autospoil, by Cristan Szmajda
   (but I have only seen the output of autospoil, not the source). */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "extern.h"
#include "artifact.h"
#include "prop.h"
#include <fcntl.h>

#define SPOILPREFIX SCOREPREFIX
#define VARIANTNAME "NetHack Fourk"
#define VERSION msgprintf("4.%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL)

static const char * semicolonjoin(const char *a, const char *b);
/* poor man's join function */

static const char * htmlheader(const char *spoilername);
static const char * spoiloname(int i);
static const char * spoilweapskill(int i);
static const char * spoilschool(int i);
static const char * spoilversus(struct artifact *art);
static const char * spoildamage(int i, boolean large, struct artifact *);
#define SDAM FALSE
#define LDAM TRUE
static const char * spoiltohit(int i, struct artifact *);
static const char * oslotname(enum objslot os);
static const char * spoilmaligntyp(int i);
static const char * spoilaligntyp(aligntyp aln);
static const char * one_attack_type(const struct attack *attk);
static const char * one_damage_type(const struct attack *attk);
static const char * spoiloneattack(const struct attack *attk);
static const char * spoilattacks(int i);
static const char * spoilmonskills(int i);
static const char * spoilresistances(uchar res, boolean convey, int i);
static const char * spoilmonsize(uchar s);
static const char * spoilmrace(int i);
static const char * spoilmonflagone(unsigned long mflags);
static const char * spoilmonflagtwo(unsigned long mflags, boolean dorace);
static const char * spoilmonflagthree(unsigned long mflags);
static const char * spoilmonflags(int i);
static void spoilobjclass(FILE *file, const char *hrname, const char *aname,
                          int classone, int classtwo);
static const char * spoilrolename(short rolepm);
static const char * spoilracename(short racepm);
static const char * spoilartalign(struct artifact *art);
static const char * spoilarteffects(struct artifact *art,
                                    unsigned long spfx, struct attack attk);
static const char * spoilartinvoke(struct artifact *art);
static const char * spoilartotherinfo(struct artifact *art);
static const char * spoilgenders(short allow);
static const char * spoilaligns(short allow);
static const char * spoilraceroles(short allow);
static const char * spoilroleraces(short allow);
static const char * attrlabel(int i);
static const char * spoilattributes(const char *labelone, const xchar *attrone,
                                    const char *labeltwo, const xchar *attrtwo,
                                    const char *labelthr, const xchar *attrthr);
static const char * spoiladvancerow(const char *label,
                                    const struct RoleAdvance *adv);
static const char * spoiladvance(const char *labelone,
                                 const struct RoleAdvance *advone,
                                 const char *labeltwo,
                                 const struct RoleAdvance *advtwo, int cutoff);
static const char * spoilspellpenalty(const char *class, const char *label,
                                      int p);
static const char * spoilrolespellcasting(int i);
static const char * spoilquestart(int i);
static const char * spoilracenotes(int i);
static const char * spoilrolenotes(int i);
static void makehtmlspoilers(void);
static void makepinobotyaml(void);

const char *at[LAST_CONTINUOUS_AT + 1] = {
[AT_NONE] = "passive",
    [AT_CLAW] = "claw",
    [AT_BITE] = "bite",
    [AT_KICK] = "kick",
    [AT_BUTT] = "butt",
    [AT_TUCH] = "touch",
    [AT_STNG] = "sting",
    [AT_HUGS] = "hug",
    [AT_ERR8] = "error_8",
    [AT_ERR9] = "error_9",
    [AT_SPIT] = "spit",
    [AT_ENGL] = "engulf",
    [AT_BREA] = "breath",
    [AT_EXPL] = "actively explode",
    [AT_BOOM] = "passively explode",
    [AT_GAZE] = "gaze",
    [AT_TENT] = "tentacle",
    [AT_SPIN] = "spin",
};
const char *ad[LAST_CONTINUOUS_AD + 1] = {
    [AD_PHYS] = "physical",
    [AD_MAGM] = "magic missile",
    [AD_FIRE] = "fire",
    [AD_COLD] = "cold",
    [AD_SLEE] = "sleep",
    [AD_DISN] = "disintegrate",
    [AD_ELEC] = "shock",
    [AD_DRST] = "strength drain",
    [AD_ACID] = "acid",
    [AD_SPC1] = "special1",
    [AD_SPC2] = "special2",
    [AD_BLND] = "blinding",
    [AD_STUN] = "stun",
    [AD_SLOW] = "slow",
    [AD_PLYS] = "paralysis",
    [AD_DRLI] = "level drain",
    [AD_DREN] = "energy drain",
    [AD_LEGS] = "leg wound",
    [AD_STON] = "petrification",
    [AD_STCK] = "stick-to-you",
    [AD_SGLD] = "gold theft",
    [AD_SITM] = "item theft",
    [AD_SEDU] = "charming",
    [AD_TLPT] = "teleportation",
    [AD_RUST] = "rust",
    [AD_CONF] = "confusion",
    [AD_DGST] = "digestion",
    [AD_HEAL] = "healing",
    [AD_WRAP] = "wrap-around",
    [AD_WERE] = "lycanthropy",
    [AD_DRDX] = "dexterity drain",
    [AD_DRCO] = "constitution drain",
    [AD_DRIN] = "intelligence drain",
    [AD_DISE] = "disease",
    [AD_DCAY] = "rotting",
    [AD_SSEX] = "seduction",
    [AD_HALU] = "hallucination",
    [AD_DETH] = "death",
    [AD_PEST] = "pestilence",
    [AD_FAMN] = "famine",
    [AD_SLIM] = "sliming",
    [AD_ENCH] = "disenchantment",
    [AD_CORR] = "corrosion",
    [AD_FLPN] = "vicarious suffering",
    [AD_SCLD] = "stinking cloud",
    [AD_PITS] = "pits",
    [AD_ICEB] = "iceblock",
    [AD_DISP] = "displacement",
    [AD_WEBS] = "web",
};

const char *
htmlheader(const char * spoilername)
{
    const char *variantname = VARIANTNAME;
    const char *version     = VERSION;
    const char *createdby   = "<!-- Generated Automatically by the NH4 Spoiler Creator -->";
    const char *copyright   = "<!-- HTML Markup by Nathan Eady is public domain or CC0 at your option -->";
    const char *csslink     = "<link rel=\"stylesheet\" type=\"text/css\" href=\"spoilers.css\" />";
    return msgprintf("<html><head><title>%s %s Spoiler</title>\n%s\n%s\n%s\n</head><body>"
                     "<p>This spoiler pertains to <span class=\"variant\">%s</span> "
                     "   <span class=\"nhversion\">version <span class=\"versionnumber\">%s</span>.\n"
                     "   <span class=\"generated\">Generated <span class=\"gendate\">%ld</span>.</span></p>",
                     variantname, spoilername, createdby, copyright, csslink, variantname, version,
                     yyyymmdd(utc_time()));
}

const char *
spoiloname(int i)
{
    const char *name = obj_descr[objects[i].oc_name_idx].oc_name;
    const char *desc = obj_descr[objects[i].oc_descr_idx].oc_descr;
    if (name && desc)
        return msgprintf("%s <span class=\"appearance\">(%s)</span>",
                         name, desc);
    else if (name)
        return name;
    else if (desc)
        return msgprintf("<span class=\"appearance\">(%s)</span>", desc);
    else return "[Unknown Object]";
}

const char *
spoilweapskill(int i)
{
    return skill_name(abs(objects[i].oc_skill));
}

const char *
spoilschool(int i)
{
    return skill_name(objects[i].oc_skill);
}

const char *
spoilversus(struct artifact *art)
{
    const struct permonst *pm;
    /* Other Relevant SPFX_ things: DBONUS */
    if (!(art->spfx & (SPFX_DBONUS | SPFX_ATTK))) {
        return (art->attk.adtyp == AD_PHYS) ? "vs all" : "vs none";
    } else if (art->spfx & SPFX_DMONS) {
        pm = &mons[(int)art->mtype];
        return msgprintf("vs %s", pm->mname);
        // TODO: improve on this
    } else if (art->spfx & SPFX_DCLAS) {
        return msgprintf("vs %c - %s",
                         ((char) def_monsyms[(int) art->mtype]),
                         monexplain[(int) art->mtype]);
    } else if (art->spfx & SPFX_DFLAG1) {
        return msgprintf("vs %s", spoilmonflagone(art->mtype));
    } else if (art->spfx & SPFX_DFLAG2) {
        return msgprintf("vs %s", spoilmonflagtwo(art->mtype, TRUE));
    } else if (art->spfx & SPFX_DALIGN) {
        return "vs cross-aligned";
    } else if (art->spfx & SPFX_ATTK) {
        return "vs non-resistant";
    }
    return "N/A";
}

const char *
spoildamage(int i, boolean large, struct artifact *art)
{
    int dmg = large ? objects[i].oc_wldam : objects[i].oc_wsdam;
    const char *dmgval_bonus = ""; /* Must be kept in sync with dmgval(). */
    const char *artibonus = (art) ?
        (art->attk.damd ? msgprintf("<span class=\"dbon\">+%s%d</span>",
                                    ((art->attk.damd == 1) ? "" : "d"),
                                    art->attk.damd)
                        : ("<span class=\"dbon dbldam\">x2</span>")) : "";
    if (large) {
        switch (i) {
        case IRON_CHAIN:
        case CROSSBOW_BOLT:
        case MORNING_STAR:
        case PARTISAN:
        case RUNESWORD:
        case ELVEN_BROADSWORD:
        case BROADSWORD:
            dmgval_bonus = "<span class=\"dmgval_bonus\">+1</span>";
        break;

        case FLAIL:
            dmgval_bonus = "<span class=\"dmgval_bonus\">+1d4</span>";
            break;

        case ACID_VENOM:
        case HALBERD:
            dmgval_bonus = "<span class=\"dmgval_bonus\">+1d6</span>";
            break;

        case BATTLE_AXE:
        case TRIDENT:
            dmgval_bonus = "<span class=\"dmgval_bonus\">+2d4</span>";
            break;

        case TSURUGI:
        case DWARVISH_MATTOCK:
        case TWO_HANDED_SWORD:
            dmgval_bonus = "<span class=\"dmgval_bonus\">+2d6</span>";
        }
    } else {
        switch (i) {
        case IRON_CHAIN:
        case CROSSBOW_BOLT:
        case MACE:
        case WAR_HAMMER:
        case FLAIL:
        case TRIDENT:
            dmgval_bonus = "<span class=\"dmgval_bonus\">+1</span>";
            break;

        case BATTLE_AXE:
        case MORNING_STAR:
        case BROADSWORD:
        case ELVEN_BROADSWORD:
        case RUNESWORD:
            dmgval_bonus = "<span class=\"dmgval_bonus\">+1d4</span>";
            break;

        case ACID_VENOM:
            dmgval_bonus = "<span class=\"dmgval_bonus\">+1d6</span>";
            break;
        }
    }
    return msgprintf("d%d%s%s", dmg, dmgval_bonus, artibonus);
}

const char *
spoiltohit(int i, struct artifact *art)
{
    const char *bonus = (art && art->attk.damn) ?
        msgprintf("<span class=\"abon\">+%d</span>", art->attk.damn) : "";
    if (objects[i].oc_hitbon > 0)
        return msgprintf("+%d%s", objects[i].oc_hitbon, bonus);
    else if (objects[i].oc_hitbon == 0)
        return bonus;
    else
        return msgprintf("%d%s", objects[i].oc_hitbon, bonus);
}

static const char *
oslotname(enum objslot os)
{
    if (os == ARM_CLOAK)
        return "cloak";
    if (os == ARM_HELM)
        return "helm";
    if (os == ARM_GLOVES)
        return "gloves";
    if (os == ARM_SHIELD)
        return "off hand";
    if (os == ARM_BOOTS)
        return "boots";
    if (os == ARM_SHIRT)
        return "shirt";
    if (os == ARM_SUIT)
        return "body armor";

    if (os == os_amul)
        return "amulet";
    if (os == os_ringl)
        return "left ring finger";
    if (os == os_ringr)
        return "right ring finger";
    if (os == os_tool)
        return "eyewear";
    
    if (os == os_saddle)
        return "saddle";
    if (os == os_carried)
        return "carried artifact";
    if (os == os_invoked)
        return "invoked artifact";

    return "";
}

static const char *
spoilarmorsize(struct objclass *oc)
{
    uchar minsize = (uchar) abs(oc->a_minsize);
    uchar maxsize = (uchar) abs(oc->a_maxsize);
    if (maxsize > minsize) {
        return msgprintf("<span class=\"range\">%s&nbsp;&mdash; %s</span>",
                         spoilmonsize(minsize), spoilmonsize(maxsize));
    } else {
        return spoilmonsize(maxsize);
    }
}

static const char *
semicolonjoin(const char *a, const char *b)
{
    if (b[0])
        return msgprintf("%s; %s", a, b);
    return a;
}

/* The way monster alignments are specified is horrible.  It is
   the way it is because of the way monster alignment interacts
   with player alignment record, which is even more horrible. */
static const char *
spoilmaligntyp(int i)
{
    aligntyp aln = mons[i].maligntyp;
    if (aln == A_NONE) return spoilaligntyp(A_NONE);
    if (aln >  0) return spoilaligntyp(A_LAWFUL);
    if (aln == 0) return spoilaligntyp(A_NEUTRAL);
    if (aln <  0) return spoilaligntyp(A_CHAOTIC);
    return spoilaligntyp(aln);
}

static const char *
spoilaligntyp(aligntyp aln)
{
    if (aln == A_NONE)
        return "<span class=\"alnmoloch\">una</span>";
    if (aln == A_LAWFUL)
        return "<span class=\"alnlaw\">law</span>";
    if (aln == A_NEUTRAL)
        return "<span class=\"alnneu\">neu</span>";
    if (aln == A_CHAOTIC)
        return "<span class=\"alncha\">cha</span>";
    return "<span class=\"error alnunknown\">ERR</span>";    
}

/* helper function for oneattack and spoiloneattack */
static const char *
one_attack_type(const struct attack *attk) {
    return ((attk->aatyp == AT_WEAP) ? "weapon" :
            (attk->aatyp == AT_MAGC) ? "spellcasting" :
            (attk->aatyp <= LAST_CONTINUOUS_AT  /* && attk->aatyp >= 0 */) ?
            at[attk->aatyp] : "mysterious");
}

/* helper function for oneattack and spoiloneattack */
static const char *
one_damage_type(const struct attack *attk)
{
    return ((attk->adtyp == AD_CLRC) ? "clerical" :
            (attk->adtyp == AD_SPEL) ? "arcane" :
            (attk->adtyp == AD_RBRE) ? "random breath" :
            (attk->adtyp == AD_SAMU) ? "amulet stealing" :
            (attk->adtyp == AD_CURS) ? "intrinsic stealing" :
            (attk->adtyp <= LAST_CONTINUOUS_AD /* && attk->adtyp >= 0 */) ?
            ad[attk->adtyp] : "unknown damage");
}

static const char *
spoiloneattack(const struct attack *attk)
{
    if (!attk->aatyp && !attk->adtyp && !attk->damn && !attk->damd)
        return "";
    return msgprintf("<span class=\"attack\">%dd%d <span class=\"aatyp\">%s</span> <span type=\"adtype\">%s</span></span>",
                     attk->damn, attk->damd,
                     one_attack_type(attk), one_damage_type(attk));
}

/* HTML-less version of the above, used for in-game lookup */
const char *
oneattack(const struct attack *attk)
{
    const char *dicestr = "";
    if (!attk->aatyp && !attk->adtyp && !attk->damn && !attk->damd)
        return NULL;
    if (attk->damn || attk->damd) {
        if (attk->damn)
            dicestr = msgprintf("%dd%d ",
                                attk->damn, attk->damd);
        else
            dicestr = msgprintf("(level+1)d%d ", attk->damd);
    }

    return msgprintf("%s%s %s", dicestr,
                     one_attack_type(attk), one_damage_type(attk));
}

/* This relies on NATTK being small and known at code-writing time.
   Otherwise we'd have to get clever with buffers or something. */
static const char *
spoilattacks(int i)
{
    return semicolonjoin(spoiloneattack(&mons[i].mattk[0]),
                         semicolonjoin(spoiloneattack(&mons[i].mattk[1]),
           semicolonjoin(spoiloneattack(&mons[i].mattk[2]),
                         semicolonjoin(spoiloneattack(&mons[i].mattk[3]),
           semicolonjoin(spoiloneattack(&mons[i].mattk[4]),
                         spoiloneattack(&mons[i].mattk[5]))))));
}

static const char *
spoilskill(const char *label, unsigned int mskill, int proficiency)
{
    short level = ((mskill / proficiency) % 4);
    if (level < 1) return "";
    const char *levdesc = ((level == 1) ? "Basic" :
                           (level == 2) ? "Skilled" : "Expert");
    return msgprintf("<span class=\"skill %sskill\">"
                     "<span class=\"label\">%s:</span> "
                     "<span class=\"level\">%s</span></span>",
                     label, label, levdesc);
}

static const char *
spoilmonskills(int i)
{
    /* If support for other skills is added to the game,
       we should add spoiler info for them here as well.
       For now, there's only wand skill: */
    return spoilskill("wand", mons[i].mskill, MP_WANDS);
}

static const char *
spoilresistances(uchar res, boolean convey, int i)
{
    return msgprintf("%s%s%s%s%s%s%s%s%s%s%s",
                     /* First, the easy ones that come from res: */
                     ((res & MR_FIRE)   ? "<span class=\"resf\" title=\"Fire\">F</span>" : ""),
                     ((res & MR_COLD)   ? "<span class=\"resc\" title=\"Cold\">C</span>" : ""),
                     ((res & MR_SLEEP)  ? "<span class=\"resz\" title=\"ZZZ = sleep\">Z</span>" : ""),
                     ((res & MR_DISINT) ? "<span class=\"resd\" title=\"Disint\">D</span>" : ""),
                     ((res & MR_ELEC)   ? "<span class=\"ress\" title=\"Shock\">S</span>" : ""),
                     ((res & MR_POISON) ? "<span class=\"resp\" title=\"Poison\">P</span>" : ""),
                     ((res & MR_ACID)   ? "<span class=\"resa\" title=\"Acid\">A</span>" : ""),
                     ((res & MR_STONE)  ? "<span class=\"resn\" title=\"stoNe\">N</span>" : ""),
                     /* corpses can also convey teleportitis, teleport control, telepathy */
                     ((convey && ((mons[i].mflags1 & M1_TPORT) != 0)) ?
                      "<span class=\"tport\" title=\"Jumpy (teleportitis)\">J</span>" : ""),
                     ((convey && ((mons[i].mflags1 & M1_TPORT_CNTRL) != 0)) ?
                      "<span class=\"tctrl\" title=\"Teleport control\">T</span>" : ""),
                     ((convey && (i == PM_FLOATING_EYE || i == PM_MIND_FLAYER ||
                                  i == PM_MASTER_MIND_FLAYER)) ?
                      "<span class=\"telepathy\" title=\"telepathy (mnemonic: ESP)\">E</span>" : ""));
    /* TODO: support MR, sickness resistance, reflection */
}

static const char *
spoilmonsize(uchar s)
{
    const char * size[8] =
        { "<span class=\"sizetiny\">tiny</span>",
          "<span class=\"sizesmall\">small</span>",
          "<span class=\"sizemedium\">medium</span>",
          "<span class=\"sizelarge\">large</span>",
          "<span class=\"huge\">huge</span>",
          "<span class=\"error unknownsize\">size 5</span>",
          "<span class=\"error unknownsize\">size 6</span>",
          "<span class=\"sizegigantic\">gigantic</span>"};
    if (s < 8)
        return size[s];
    return "<span class=\"error unknownsize\">unknown</span>";
}

static const char *
spoilmrace(int i)
{
    int race = mons[i].mflagsr;
    if (race == MRACE_NONE)
        return "<span class=\"mrace norace\">N/A</span>";
    if (race == MRACE_HUMAN)
        return "<span class=\"mrace humanrace\">human</span>";
    if (race == MRACE_ELF)
        return "<span class=\"mrace elfrace\">elf</span>";
    if (race == MRACE_DWARF)
        return "<span class=\"mrace dwarfrace\">dwarf</span>";
    if (race == MRACE_GNOME)
        return "<span class=\"mrace gnomerace\">gnome</span>";
    if (race == MRACE_ORC)
        return "<span class=\"mrace orcrace\">orc</span>";
    if (race == MRACE_FAIRY)
        return "<span class=\"mrace fairyrace\">fairy</span>";
    if (race == MRACE_RODENT)
        return "<span class=\"mrace rodentrace\">rodent</span>";
    if (race == MRACE_GIANT)
        return "<span class=\"mrace giantrace\">giant</span>";

    return "<span class=\"mrace unknownrace error\">UNKNOWN</span>";
}

static const char *
spoilmonflagone(unsigned long mflags)
{
    return msgprintf("%s%s%s%s%s%s%s%s" "%s%s%s%s%s%s%s"
                     "%s%s%s%s%s%s%s%s" "%s%s%s%s%s%s%s%s%s",
                     /* M1 least significant byte */
                     ((mflags & M1_FLY)       ? "<span class=\"flgfly\">Fly</span> " : ""),
                     ((mflags & M1_SWIM)      ? "<span class=\"flgswim\">Swim</span> " : ""),
                     ((mflags & M1_AMORPHOUS) ? "<span class=\"flgamorph\">Amorph</span> " : ""),
                     ((mflags & M1_WALLWALK)  ? "<span class=\"flgwwalk\">Wallwalk</span> " : ""),
                     ((mflags & M1_CLING)     ? "<span class=\"flgcling\">Cling</span> " : ""),
                     (((mflags & M1_TUNNEL) && !(mflags & M1_NEEDPICK)) ?
                                                         "<span class=\"flgtunnel\">Tunnel</span> " : ""),
                     ((mflags & M1_NEEDPICK)  ? "<span class=\"flgpick\">Pick</span> " : ""),
                     ((mflags & M1_CONCEAL)   ? "<span class=\"flgconceal\">Conceal</span> " : ""),
                     /* M1 second least byte */
                     ((mflags & M1_HIDE)       ? "<span class=\"flghide\">Hide</span> " : ""),
                     ((mflags & M1_AMPHIBIOUS) ? "<span class=\"flgamphib\">Amphib</span> " : ""),
                     ((mflags & M1_BREATHLESS) ? "<span class=\"flgbreathless\">Breathless</span> " : ""),
                     ((mflags & M1_NOTAKE)     ? "<span class=\"flgnotake\">NoTake</span> " : ""),
                     ((mflags & M1_NOEYES)     ? "<span class=\"flgnoeyes\">NoEyes</span> " : ""),
                     /* special handling for M1_NOLIMBS because of wonky bit
                        usage inherited from vanilla, wherein two bits are used
                        non-independently to hold two binary properties */
                     (((mflags & M1_NOLIMBS) == M1_NOLIMBS)
                                               ? "<span class=\"flgfly\">NoLimbs</span> " :
                      ((mflags & M1_NOHANDS)   ? "<span class=\"flgfly\">NoHands</span> " : "")),
                     ((mflags & M1_NOHEAD)     ? "<span class=\"flgnohead\">NoHead</span> " : ""),
                     /* M1 third byte */
                     ((mflags & M1_MINDLESS)   ? "<span class=\"flgmindless\">Mindless</span> " : ""),
                     ((mflags & M1_HUMANOID)   ? "<span class=\"flghumanoid\">Humanoid</span> " : ""),
                     ((mflags & M1_ANIMAL)     ? "<span class=\"flganimal\">Animal</span> " : ""),
                     ((mflags & M1_SLITHY)     ? "<span class=\"flgslithy\">Slithy</span> " : ""),
                     ((mflags & M1_UNSOLID)    ? "<span class=\"flgunsolid\">Unsolid</span> " : ""),
                     ((mflags & M1_THICK_HIDE) ? "<span class=\"flgthickhide\">ThickHide</span> " : ""),
                     ((mflags & M1_OVIPAROUS)  ? "<span class=\"flgoviparous\">Oviparous</span> " : ""),
                     ((mflags & M1_REGEN)      ? "<span class=\"flgregen\">Regen</span> " : ""),
                     /* M1 most significant byte */
                     ((mflags & M1_SEE_INVIS)  ? "<span class=\"flgseeinvis\">SeeInvis</span> " : ""),
                     ((mflags & M1_TPORT)      ? "<span class=\"flgtport\">Tport</span> " : ""),
                     ((mflags & M1_TPORT_CNTRL)? "<span class=\"flgtportcntrl\">TeleCtrl</span> " : ""),
                     ((mflags & M1_ACID)       ? "<span class=\"flgacid\">Acidic</span> " : ""),
                     ((mflags & M1_POIS)       ? "<span class=\"flgpois\">Poisonous</span> " : ""),
                     (((mflags & M1_CARNIVORE) && !(mflags & M1_HERBIVORE)) ?
                                                          "<span class=\"flgcarnivore\">Carnivore</span> " : ""),
                     (((mflags & M1_HERBIVORE) && !(mflags & M1_CARNIVORE))  ?
                                                          "<span class=\"flgherbivore\">Herbivore</span> " : ""),
                     ((mflags & M1_OMNIVORE)   ? "<span class=\"flgomnivore\">Omnivore</span> " : ""),
                     ((mflags & M1_METALLIVORE)? "<span class=\"flgmetallivore\">Metallivore</span> " : ""));
}

static const char *
spoilmonflagtwo(unsigned long mflags, boolean dorace)
{
    return msgprintf("%s%s%s%s%s"       "%s%s%s%s%s%s"
                     "%s%s%s%s%s%s%s%s" "%s%s%s%s%s%s%s%s",
                     /* M2 least significant byte */
                     ((mflags & M2_NOPOLY)     ? "<span class=\"flgnopoly\">NoPoly</span> " : ""),
                     ((mflags & M2_UNDEAD)     ? "<span class=\"flgundead\">Undead</span> " : ""),
                     ((mflags & M2_WERE)       ? "<span class=\"flgwere\">Lycanthrope</span> " : ""),
                     /* human, dwarf, elf, orc, and gnome are moved to MRACE, q.v. */
                     /* However, for artifact purposes, a couple of them still have an M2_ flag,
                        which we may want to show (e.g., for an SPFX_DFLAG2 artifact) */
                     ((dorace && (mflags & M2_ELF))
                                               ? "<span class=\"flgelf\">Elf</span> " : ""),
                     ((dorace && (mflags & M2_ORC))
                                               ? "<span class=\"flgorc\">Orc</span> " : ""),
                     /* M2 second least byte */
                     ((mflags & M2_DEMON)      ? "<span class=\"flgdemon\">Demon</span> " : ""),
                     ((mflags & M2_MERC)       ? "<span class=\"flgmerc\">Mercinary</span> " : ""),
                     ((mflags & M2_LORD)       ? "<span class=\"flglord\">Lord</span> " : ""),
                     ((mflags & M2_PRINCE)     ? "<span class=\"flgprince\">Prince</span> " : ""),
                     ((mflags & M2_MINION)     ? "<span class=\"flgminion\">Minion</span> " : ""),
                     ((mflags & M2_GIANT)      ? "<span class=\"flggiant\">Giant</span> " : ""),
                      /* There are two open bits here */
                      /* M2 third byte */
                     ((mflags & M2_MALE)       ? "<span class=\"flgmale\">Male</span> " : ""),
                     ((mflags & M2_FEMALE)     ? "<span class=\"flgfemale\">Female</span> " : ""),
                     ((mflags & M2_NEUTER)     ? "<span class=\"flgneuter\">Neuter</span> " : ""),
                     ((mflags & M2_PNAME)      ? "<span class=\"flgpname\">ProperName</span> " : ""),
                     ((mflags & M2_HOSTILE)    ? "<span class=\"flghostile\">Hostile</span> " : ""),
                     ((mflags & M2_PEACEFUL)   ? "<span class=\"flgpeaceful\">Peaceful</span> " : ""),
                     ((mflags & M2_DOMESTIC)   ? "<span class=\"flgdomestic\">Domestic</span> " : ""),
                     ((mflags & M2_WANDER)     ? "<span class=\"flgwander\">Wander</span> " : ""),
                     /* M2 most significant byte */
                     ((mflags & M2_STALK)      ? "<span class=\"flgstalk\">Stalk</span> " : ""),
                     ((mflags & M2_NASTY)      ? "<span class=\"flgnasty\">M2_NASTY</span> " : ""),
                     ((mflags & M2_STRONG)     ? "<span class=\"flgstrong\">Strong</span> " : ""),
                     ((mflags & M2_ROCKTHROW)  ? "<span class=\"flgrockthrow\">Boulders</span> " : ""),
                     ((mflags & M2_GREEDY)     ? "<span class=\"flggreedy\">Greedy</span> " : ""),
                     ((mflags & M2_JEWELS)     ? "<span class=\"flgjewels\">Jewels</span> " : ""),
                     ((mflags & M2_COLLECT)    ? "<span class=\"flgcollect\">Collects</span> " : ""),
                     ((mflags & M2_MAGIC)      ? "<span class=\"flgmagic\">MagicItems</span> " : ""));
}

static const char *
spoilmonflagthree(unsigned long mflags)
{
    return msgprintf("%s%s%s%s%s%s%s%s" "%s%s%s%s%s%s",
                     /* M3 least significant byte */
                     (((mflags & M3_WANTSAMUL) && !((mflags & M3_COVETOUS) == M3_WANTSALL)) ?
                                                          "<span class=\"flgwantsamul\">Amulet</span> " : ""),
                     (((mflags & M3_WANTSBELL) && !((mflags & M3_COVETOUS) == M3_WANTSALL)) ?
                                                          "<span class=\"flgwantsbell\">Bell</span> " : ""),
                     (((mflags & M3_WANTSBOOK) && !((mflags & M3_COVETOUS) == M3_WANTSALL)) ?
                                                          "<span class=\"flgwantsbook\">Book</span> " : ""),
                     (((mflags & M3_WANTSCAND) && !((mflags & M3_COVETOUS) == M3_WANTSALL)) ?
                                                          "<span class=\"flgwantscand\">Candellabrum</span> " : ""),
                     (((mflags & M3_WANTSARTI) && !((mflags & M3_COVETOUS) == M3_WANTSALL)) ?
                                                          "<span class=\"flgwantsarti\">Artifact</span> " : ""),
                     (((mflags & M3_COVETOUS) == M3_WANTSALL) ?
                                                          "<span class=\"flgcovetous\">Covetous</span> " : ""),
                     /* There's an open bit here */
                     ((mflags & M3_WAITFORU)   ? "<span class=\"flgwaitforu\">WaitForYou</span> " : ""),
                     ((mflags & M3_CLOSE)      ? "<span class=\"flgclose\">Close</span> " : ""),
                     /* M3 second byte */
                     ((mflags & M3_INFRAVISION)  ? "<span class=\"flginfravision\">InfraVision</span> " : ""),
                     ((mflags & M3_INFRAVISIBLE) ? "<span class=\"flginfravisible\">InfraVisible</span> " : ""),
                     ((mflags & M3_SCENT)        ? "<span class=\"flgscent\">Scent</span> " : ""),
                     ((mflags & M3_DISPLACES)    ? "<span class=\"flgdisplaces\">Displaces</span> " : ""),
                     ((mflags & M3_BLINKAWAY)    ? "<span class=\"flgblinkaway\">BlinkAway</span> " : ""),
                     ((mflags & M3_VANDMGRDUC)   ? "<span class=\"flgvandmgrduc\">VanDmgReduce</span> " : "")
        );
}

static const char *
spoilmonflags(int i)
{
    return msgprintf("%s%s%s",
                     spoilmonflagone((unsigned long) mons[i].mflags1),
                     spoilmonflagtwo((unsigned long) mons[i].mflags2, FALSE),
                     spoilmonflagthree((unsigned long) mons[i].mflags3));
}

static void
spoilobjclass(FILE *file, const char * hrname, const char * aname,
                 int classone, int classtwo)
{
    int i;
    const char * extrafield = (classone == FOOD_CLASS) ?
        "<th class=\"numeric extrafield nutrition\">Nutr</th>" :
        (classone == SPBOOK_CLASS) ?
        msgprintf("<th class=\"spschool\">school</th>"
                  "<th class=\"numeric extrafield splev\">%s</th>",
                  "splev") : "";
    fprintf(file, "\n<h1><a name=\"%s\">%s</a></h1>\n"
            "<table id=\"%s\"><thead>\n  "
            "<tr><th id=\"object\">object</th>"
            "<th class=\"material\">mat</th>%s"
            "<th class=\"numeric weight\">wt</th>"
            "<th class=\"numeric price\">zm</th>"
            "</tr>\n</thead><tbody>\n",
            aname, hrname, aname, extrafield);
    for (i = 0; !i || objects[i].oc_class != ILLOBJ_CLASS; i++) {
        if (objects[i].oc_class != classone &&
            objects[i].oc_class != classtwo)
            continue;
        const char * extravalue = (classone == FOOD_CLASS) ?
            msgprintf("<td class=\"numeric extrafield nutrition\">%d</td>",
                      objects[i].oc_nutrition) :
            (classone == SPBOOK_CLASS) ?
            msgprintf("<td class=\"spschool\">%s</td>"
                      "<td class=\"numeric extrafield splev\">%d</td>",
                      spoilschool(i), objects[i].oc_level) : "";
        fprintf(file, "<tr><td class=\"object\" id=\"obj%d\">%s</td>"
                "<td class=\"material\">%s</td>%s"
                "<td class=\"numeric weight\">%d</td>"
                "<td class=\"numeric price\">%d</td>"
                "</tr>\n",
                i, spoiloname(i), material_name(objects[i].oc_material),
                extravalue, objects[i].oc_weight, objects[i].oc_cost);
    }

    fprintf(file, "</tbody></table>\n");
}

const char *
spoilrolename(short rolepm)
{
    if (rolepm == NON_PM)
        return "";
    return msgprintf("<span class=\"role\">%s</span> ", mons[rolepm].mname);
}

const char *
spoilracename(short racepm)
{
    if (racepm == NON_PM)
        return "";
    return msgprintf("<span class=\"race\">%s</span> ", mons[racepm].mname);
}

const char *
spoilartalign(struct artifact *art)
{
    return msgprintf("%s%s%s%s",
                     ((art->spfx & SPFX_INTEL) ?
                      "<span class=\"artint\">Int</span> " : ""),
                     msgprintf("<span class=\"artaln\">%s</span> ",
                               spoilaligntyp(art->alignment)),
                     spoilrolename(art->role),
                     spoilracename(art->race));
}

static const char *
spoilarteffects(struct artifact *art, unsigned long spfx, struct attack attk)
{
    return msgprintf("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s %s%s%s",
                     ((spfx & SPFX_SEEK) ?
                      "<span class=\"spfx spfxseek\">+n search</span> " : ""),
                     ((spfx & SPFX_WARN) ?
                      "<span class=\"spfx spfxwarn\">warn</span> " : ""),
                     ((spfx & SPFX_DRLI) ?
                      "<span class=\"spfx spfxdrli\">drain</span>" : ""),
                     ((spfx & SPFX_SEARCH) ?
                      "<span class=\"spfx spfxsearch\">autosrch</span> " : ""),
                     ((spfx & SPFX_BEHEAD) ?
                      "<span class=\"spfx spfxbehead\">vorpal</span> " : ""),
                     ((spfx & SPFX_HALRES) ?
                      msgprintf("<span class=\"spfx spfxhalres\">%s%s</span> ",
                                "<abbr title=\"hallucination resistance\">",
                                "halres</abbr>"): ""),
                     ((spfx & SPFX_ESP) ?
                      "<span class=\"spfx spfxesp\">ESP</span> " : ""),
                     ((spfx & SPFX_STLTH) ?
                      "<span class=\"spfx spfxstealth\">stealth</span> " : ""),
                     ((spfx & SPFX_REGEN) ?
                      "<span class=\"spfx spfxregen\">regen</span> " : ""),
                     ((spfx & SPFX_EREGEN) ?
                      "<span class=\"spfx spfxeregen\">eregen</span> " : ""),
                     ((spfx & SPFX_HSPDAM) ?
                      "<span class=\"spfx spfxhspdam\">half-spell</span> " :
                      ""),
                     ((spfx & SPFX_HPHDAM) ?
                      "<span class=\"spfx spfxhphdam\">half-phys</span> " :
                      ""),
                     ((spfx & SPFX_TCTRL) ?
                      "<span class=\"spfx spfxtctrl\">T-ctrl</span> " : ""),
                     ((spfx & SPFX_LUCK) ?
                      "<span class=\"spfx spfxluck\">luck</span> " : ""),
                     ((spfx & SPFX_XRAY) ?
                      "<span class=\"spfx spfxxray\">xray</span> " : ""),
                     ((spfx & SPFX_REFLECT) ?
                      "<span class=\"spfx spfxreflect\">reflect</span> " : ""),
                     ((spfx & SPFX_STRM) ?
                      "<span class=\"spfx spfxstrm\">storm</span> " : ""),
                     ((spfx & SPFX_FREEA) ?
                      "<span class=\"spfx spfxfreea\">free act.</span> " : ""),
                     ((spfx & SPFX_PSHCH) ?
                      "<span class=\"spfx spfxprotshch\"><abbr title=\"protection from shape changers\">PSCH</abbr></span> " : ""),
                     msgprintf("<span class=\"artattk\">%s</span>",
                               /* TODO: handle attk.damn and attk.damd */
                               (attk.adtyp && attk.adtyp == AD_MAGM) ? "MR" :
                               (attk.adtyp && attk.adtyp < 47) ?
                               ad[attk.adtyp] : "")
        );
}

const char *
spoilartinvoke(struct artifact *art)
{
    uchar i = art->inv_prop;
    switch(i) {
    case 0:
        return "";
    case TAMING:
        return "<span class=\"invoke invoketaming\">taming</span>";
    case HEALING:
        return "<span class=\"invoke invokehealing\">healing</span>";
    case ENERGY_BOOST:
        return "<span class=\"invoke invokeenergyboost\">power</span>";
    case UNTRAP:
        return "<span class=\"invoke invokeuntrap\">untrap</span>";
    case CHARGE_OBJ:
        return "<span class=\"invoke invokecharging\">charging</span>";
    case LEV_TELE:
        return "<span class=\"invoke invokelevport\">levelport</span>";
    case CREATE_PORTAL:
        return "<span class=\"invoke invokebranchport\">branchport</span>";
    case ENLIGHTENING:
        return "<span class=\"invoke invokeenlight\">enlightenment</span>";
    case CREATE_AMMO:
        return "<span class=\"invoke invokeammo\">create ammo</span>";
    /* TODO: handle cases FIRE_RES through LAST_PROP */
    /* For now I'm just doing the ones used by existing artifacts. */
    case INVIS:
        return "<span class=\"invoke invokeinvis\">invisibility</span>";
    case LEVITATION:
        return "<span class=\"invoke invokelev\">levitation</span>";
    case CONFLICT:
        return "<span class=\"invoke invokeconflict\">conflict</span>";
    case UNCURSE_INVK:
        return "<span class=\"invoke invokeuncurse\">remove curse</span>";
    default:
        return msgprintf("<!-- unknown invoke property -->%d", (int) i);
    }
}

const char *
spoilartotherinfo(struct artifact *art)
{
    return msgprintf("%s%s%s",
                     ((art->spfx & SPFX_NOGEN) ?
                      "<span class=\"artother artnogen\">nogen</span> " : ""),
                     (!(art->spfx & SPFX_RESTR) ?
                      "<span class=\"artother artcanname\">#name</span> " : ""),
                     ((art->spfx & SPFX_SPEAK) ?
                      "<span class=\"artother artspeak\">speaks</span> " : ""));
}

const char *
spoilgenders(short allow)
{
    short g = (allow & ROLE_GENDMASK);
    return msgprintf("<span class=\"genders\">%s %s %s</span>",
     ((g & ROLE_MALE)   ?
     "<span class=\"flgmale\"><abbr title=\"male\">Mal</abbr></span>"   : ""),
     ((g & ROLE_FEMALE) ?
     "<span class=\"flgfemale\"><abbr title=\"female\">Fem<abbr></span>": ""),
     ((g & ROLE_NEUTER) ?
     "<span class=\"flgneuter\"><abbr title=\"neuter\">Neut</abbr></span>"
                                                                        : ""));
}

const char *
spoilaligns(short allow) {
    short a = (allow & ROLE_ALIGNMASK);
    return msgprintf("<span class=\"aligns\">%s %s %s</span>",
                     ((a & ROLE_LAWFUL)  ? spoilaligntyp(A_LAWFUL)  : ""),
                     ((a & ROLE_NEUTRAL) ? spoilaligntyp(A_NEUTRAL) : ""),
                     ((a & ROLE_CHAOTIC) ? spoilaligntyp(A_CHAOTIC) : ""));
}

const char *
spoilroleraces(short allow) {
    const char *thelist = "";
    int i, j = 0;
    for (i = 0; races[i].filecode; i++) {
        if ((races[i].selfmask) & allow) {
            thelist = msgprintf("%s%s<span class=\"race%s\">"
                                "<abbr title=\"%s\">%s</abbr></span>",
                                thelist, ((j++ > 0) ? ", " : ""),
                                races[i].filecode, races[i].noun,
                                races[i].filecode);
        }
    }
    return msgprintf("<span class=\"races\">%s</span>", thelist);
}

const char *
spoilraceroles (short selfmask) {
    const char *thelist = "";
    int i, j = 0;
    for (i = 0; roles[i].filecode; i++) {
        short thisrole = (roles[i].allow & ROLE_RACEMASK);
        if (thisrole & selfmask) {
            thelist = msgprintf("%s%s<span class=\"role%s\">"
                                "<abbr title=\"%s\">%s</abbr></span>",
                                thelist, ((j++ > 0) ? ", " : ""),
                                roles[i].name.m, roles[i].name.m,
                                roles[i].filecode);
        }
    }
    return msgprintf("<span class=\"roles\">%s</span>", thelist);
}

const char *
attrlabel (int i) {
    if      (i == A_STR) { return "Str"; }
    else if (i == A_INT) { return "Int"; }
    else if (i == A_WIS) { return "Wis"; }
    else if (i == A_DEX) { return "Dex"; }
    else if (i == A_CON) { return "Con"; }
    else if (i == A_CHA) { return "Cha"; }
    else return msgprintf("%d??", i);
}

const char *
spoilattributes(const char *labelone, const xchar *attone,
                const char *labeltwo, const xchar *attrtwo,
                const char *labelthr, const xchar *attrthr) {
    int i;
    const char *headers = "<th></th>";
    const char *rowone  = msgprintf("<th>%s:</th>", labelone);
    const char *rowtwo  = msgprintf("<th>%s:</th>", labeltwo);
    const char *rowthr  = attrthr ? msgprintf("<th>%s:</th>", labelthr) : "";
    for (i = 0; i < A_MAX; i++) {
        headers = msgprintf("%s<th>%s</th>", headers, attrlabel(i));
        rowone  = msgprintf("%s<td class=\"numeric attr%s\">%d</td>",
                            rowone, attrlabel(i), attone[i]);
        rowtwo  = msgprintf("%s<td class=\"numeric attr%s\">%d</td>",
                            rowtwo, attrlabel(i), attrtwo[i]);
        if (attrthr)
            rowthr = msgprintf("%s<td class=\"numeric attr%s\">%s%d</td>",
                               rowthr, attrlabel(i),
                               ((attrthr[i] > 0) ? "+" : ""),  attrthr[i] - 1);
    }
    return msgprintf("<table class=\"attributes\"><thead>\n"
                     "  <tr>%s</tr>\n"
                     "</thead><tbody>\n"
                     "  <tr class=\"attrow%s\">%s</tr>\n"
                     "  <tr class=\"attrow%s\">%s</tr>\n"
                     "%s"
                     "</tbody></table>",
                     headers, labelone, rowone, labeltwo, rowtwo,
                     (attrthr ? msgprintf("  <tr class=\"attrow%s\">%s</tr>\n",
                                          labelthr, rowthr) : ""));
}

const char *
spoiladvancerow(const char *label, const struct RoleAdvance *adv) {
    return msgprintf("<tr class=\"adv%s\"><th class=\"label\">%s:</th>"
                     "<td class=\"numeric advin\">"
                     "    <span class=\"advfix advinfix\">%d</span>"
                     "    <span class=\"advrnd advinrnd\">+d%d</span></td>"
                     "<td class=\"numeric advlo\">"
                     "    <span class=\"advfix advlofix\">%d</span>"
                     "    <span class=\"advrnd advlornd\">+d%d</span></td>"
                     "<td class=\"numeric advhi\">"
                     "    <span class=\"advfix advhifix\">%d</span>"
                     "    <span class=\"advrnd advhirnd\">+d%d</span></td>"
                     "</tr>", label, label, adv->infix, adv->inrnd,
                     adv->lofix, adv->lornd, adv->hifix, adv->hirnd);
}
const char *
spoiladvance(const char *labelone, const struct RoleAdvance *advone,
             const char *labeltwo, const struct RoleAdvance *advtwo,
             int cutoff)
{
    return msgprintf("<table><thead>"
                     "  <tr><th></th>"
                     "      <th class=\"label\">init</th>"
                     "      <th class=\"label\">early gain</th>"
                     "      <th class=\"label\">%s</th>"
                     "  </tr>"
                     "</thead><tbody>"
                     "   %s"
                     "   %s"
                     "</tbody></table>\n",
                     (cutoff ? msgprintf("gain &gt;XL%d", cutoff) :
                      "late gain"),
                     spoiladvancerow(labelone, advone),
                     spoiladvancerow(labeltwo, advtwo));
}

const char *
spoilspellpenalty(const char *class, const char *label, int p)
{
    if (p == 0) { return ""; }
    const char *sign = (p < 0) ? "<span class=\"penaltyminus\">-</span>" :
                                 "<span class=\"penaltyplus\">+</span>";
    const char *signclass = (p < 0) ? " bonus" : (p > 0) ? " malus" : "nil";
    return msgprintf("<span class=\"%s%s\">%s: "
                     "%s<span class=\"number\">%d</span></span>",
                     class, signclass, label, sign, abs(p));
}

const char *
spoilrolespellcasting(int i)
{
    return msgprintf("<span class=\"rolespellcasting\">%s %s %s %s</span>",
                     spoilspellpenalty("base", "Base", roles[i].spelbase),
                     spoilspellpenalty("armr", "Armor",  roles[i].spelarmr),
                     spoilspellpenalty("shld", "Shield", roles[i].spelshld),
                     spoilspellpenalty("heal", "Heal", roles[i].spelheal));
}

const char *
spoilquestart(int i)
{
    const struct artifact *art = &artilist[roles[i].questarti];
    return art->name;
}

const char *
spoilracenotes(int i)
{
    int num = races[i].malenum ? races[i].malenum : races[i].malenum;
    switch (num) {
    case PM_HUMAN:
        return "A largely unremarkable but surprisingly flexible race,\n"
            "found throughout the overworld, capable of most tasks.";
    case PM_ELF:
        return "Intelligent and long-lived forest dwellers.";
    case PM_DWARF:
        /* A reference to another game: */
        return "A short, sturdy creature fond of drink and industry.";
    case PM_GNOME:
        return "Small but intelligent creatures of the earth, able to see \n"
            "   in the dark places underground.";
    case PM_GIANT:
        return "So large they can hurl giant boulders around, and wield in\n"
            "   one hand weapons that would require two hands for anyone else.";
    case PM_ORC:
        return "Crude humanoid creatures, notably resistant to poison.";
    case PM_SYLPH:
        return "Creatures of faerie, able to regenerate by drawing power \n"
            "   from the world around them, not easily fooled by appearances.";
    case PM_SCURRIER:
        return "Intelligent rodents, fast and able to dig without tools,\n"
            "   but too small to wear most armor.";
    case PM_VALKYRIE:
        return "Winged warrior maidens, formerly charged with selecting the\n"
            "   souls of brave warriors for Valhalla.";
    default:
        return "<!-- This space unintentionally left blank -->";
    }
}

const char *
spoilrolenotes(int i)
{
    int num = (roles[i].malenum) ? (roles[i].malenum) : (roles[i].femalenum);
    switch (num) {
    case PM_ARCHEOLOGIST:
        return "Excavators, seeking lost treasures and clues about former\n"
            "   inhabitants of the dungeon.";
    case PM_BARBARIAN:
        return "Mighty warriors, good with heavy weapons but averse to\n"
            "   books and magic.";
    case PM_CAVEMAN:
        return "Use sling, make stones hit bad thing on head.\n"
            "   Hit bad thing with stick.  Bam.";
    case PM_HEALER:
        return "Students of the medicinal arts, able to heal themselves,\n"
            "   others, and even animals.";
    case PM_SHIELDMAIDEN:
    case PM_HOPLITE:
        return "Shield-bearing, spear-wielding warriors from the frozen white\n"
            "   north, smart enough to make limited use of books and magic.";
    case PM_KNIGHT:
        return "Formally enlisted cavalry, well equipped by their patrons\n"
            "   but bound by the standards of chivalry.";
    case PM_MONK:
        return "Students of mysticism, martial arts, and magic.";
    case PM_PRIEST:
        return "Clerics, trained by the temple, are able to sense blessings\n"
            "as well as curses but are not skilled with edged weapons.";
    case PM_ROGUE:
        return "Skirting the law makes these thieves skilled in the art of\n"
            "hiding.  Good with daggers.";
    case PM_RANGER:
        return "Wandering hunters from the wild lands, skilled at archery.";
    case PM_SAMURAI:
        return "Brave followers of the 'bushido' code of warrior conduct,\n"
            "   capable of using both the katana and the yumi, a long bow.";
    case PM_TOURIST:
        return "Visitors from far away, seeking adventure, eager to\n"
            "experience many new things.";
    case PM_WIZARD:
        return "Students of the magical arts, seeking power and knowledge.";
    default:
        return "<!-- This space unintentionally left blank -->";
    }
}

void
makehtmlspoilers(void)
{
    FILE *outfile;
    int fd = open_datafile("weapon-spoiler.html",
                           O_CREAT | O_WRONLY, SPOILPREFIX);
    int i;
    struct artifact *art;
    char *headrow = "";
    
    /* ######################## Weapons ######################## */

    if (fd < 0) {
        pline(msgc_debug, "Failed to write weapon spoiler.  Is it writable?");
        return;
    }

    if (change_fd_lock(fd, FALSE, LT_WRITE, 10)) {
        outfile = fdopen(fd, "w");
        fprintf(outfile, "%s", htmlheader("Weapons"));
        fprintf(outfile, "\n<table id=\"weapons\"><thead>\n  "
                "<tr><th class=\"object\">weapon</th>"
                "<th class=\"artifact\">artifact</th>"
                "<th class=\"skill\">skill</th>"
                "<th class=\"material\">mat</th>"
                "<th class=\"numeric tohit\">hit</th>"
                "<th class=\"damage sdam\">sdam</th>"
                "<th class=\"damage ldam\">ldam</th>"
                "<th class=\"numeric weight\">wt</th>"
                "<th class=\"numeric price\">zm</th>"
                "<th class=\"notes wpnnotes\">notes</th>"
                "</tr>\n</thead><tbody>\n");
        for (i = 0; !i || objects[i].oc_class != ILLOBJ_CLASS; i++) {
            if (objects[i].oc_class != WEAPON_CLASS &&
                (objects[i].oc_class != TOOL_CLASS ||
                 objects[i].oc_skill == P_NONE))
                continue;
            fprintf(outfile, "  <tr><td colspan=\"2\" class=\"object\">%s</td>"
                    "<td class=\"skill\">%s</td>"
                    "<td class=\"material\">%s</td>"
                    "<td class=\"numeric tohit\">%s</td>"
                    "<td class=\"damage sdam\">%s</td>"
                    "<td class=\"damage ldam\">%s</td>"
                    "<td class=\"numeric weight\">%d</td>"
                    "<td class=\"numeric price\">%d</td>"
                    "<td class=\"notes wpnnotes\">%s</td>"
                    "</tr>\n",
                    spoiloname(i), spoilweapskill(i),
                    material_name(objects[i].oc_material), spoiltohit(i, NULL),
                    spoildamage(i, SDAM, NULL), spoildamage(i, LDAM, NULL),
                    objects[i].oc_weight, objects[i].oc_cost,
                    (objects[i].oc_bimanual ?
                     "<span class=\"bimanual\" title=\"two-handed\">2H</span>"
                     : ""));
            /* Now check for artifacts with this base item */
            for (art = artilist + 1; art->otyp; art++) {
                if (art->otyp == i) {
                    /* TODO: add info about which kinds of monsters
                             the damage bonus applies against. */
                    fprintf(outfile, "<tr><td class=\"object\"></td>"
                            "<td class=\"artifact\">%s</td>"
                            "<td class=\"skill\">%s</td>"
                            "<td class=\"material\">%s</td>"
                            "<td class=\"numeric tohit\">%s</td>"
                            "<td class=\"damage sdam\">%s</td>"
                            "<td class=\"damage ldam\">%s</td>"
                            "<td class=\"numeric weight\">%d</td>"
                            "<td class=\"numeric price\">%d</td>"
                            "<td class=\"notes wpnnotes artinotes\">%s"
                            " <span class=\"versus\">%s</span></td>"
                            "</tr>", art->name, spoilweapskill(i),
                            material_name(objects[i].oc_material),
                            spoiltohit(i, art), spoildamage(i, SDAM, art),
                            spoildamage(i, LDAM, art), objects[i].oc_weight,
                            objects[i].oc_cost,
                            (objects[i].oc_bimanual ?
                             "<span class=\"bimanual\" title=\"two-handed\">"
                             "2H</span>" : ""),
                            spoilversus(art));
                }
            }
        }
        fprintf(outfile, "\n</tbody></table>\n");
        change_fd_lock(fd, FALSE, LT_NONE, 0);
        fclose(outfile);
    }

    /* ######################## Armor ######################## */
    fd = open_datafile("armor-spoiler.html",
                       O_CREAT | O_WRONLY, SPOILPREFIX);
    
    if (fd < 0) {
        pline(msgc_debug, "Failed to write armor spoiler.  Is it writable?");
        return;
    }
    
    if (change_fd_lock(fd, FALSE, LT_WRITE, 10)) {
        int firstarmor, lastslot = -1;
        outfile = fdopen(fd, "w");
        fprintf(outfile, "%s", htmlheader("Armor"));
        /* navbar at top */
        for (firstarmor = 0; !firstarmor ||
                 objects[firstarmor].oc_class != ARMOR_CLASS; firstarmor++)
            ;
        fprintf(outfile, "<div class=\"nav\">Jump to: ");
        for (i = firstarmor; objects[i].oc_class == ARMOR_CLASS; i++)
            if (objects[i].oc_armcat != lastslot) {
                fprintf(outfile, "<a href=\"#obj%d\">%s</a> ",
                        i, oslotname(objects[i].oc_armcat));
                lastslot = objects[i].oc_armcat;
            }
        fprintf(outfile, "</div>");
        /* then the actual armor table */

        fprintf(outfile, "\n<table id=\"armor\"><thead>\n  "
                "<tr><th class=\"slot\">slot</th>"
                "<th class=\"object\">armor</th>"
                "<th class=\"numeric mc\">MC</th>"
                "<th class=\"numeric ac\">def</th>"
                "<th class=\"material\">mat</th>"
                "<th class=\"size\">fits</th>"
                "<th class=\"numeric weight\">wt</th>"
                "<th class=\"numeric price\">zm</th>"
                "</tr>\n</thead><tbody>\n");

        for (i = 0; !i || objects[i].oc_class != ILLOBJ_CLASS; i++) {
            if (objects[i].oc_class != ARMOR_CLASS)
                continue;
            fprintf(outfile, "<tr><td class=\"slot\">%s</td>"
                    "<td class=\"object\" id=\"obj%d\">%s</td>"
                    "<td class=\"numeric mc\">%s</td>"
                    "<td class=\"numeric ac\">%d</td>"
                    "<td class=\"material\">%s</td>"
                    "<td class=\"armorsize\">%s</td>"
                    "<td class=\"numeric weight\">%d</td>"
                    "<td class=\"numeric price\">%d</td>"
                    "</tr>\n",
                    oslotname(objects[i].oc_armcat), i, spoiloname(i),
                    (objects[i].a_can ?
                     msgprintf("MC%d", objects[i].a_can) : ""),
                    objects[i].a_ac, material_name(objects[i].oc_material),
                    spoilarmorsize(&objects[i]),
                    objects[i].oc_weight, objects[i].oc_cost);
        }


        fprintf(outfile, "\n</tbody></table>\n");
        change_fd_lock(fd, FALSE, LT_NONE, 0);
        fclose(outfile);
    }

    /* ######################### Artifacts ######################### */
    fd = open_datafile("artifact-spoiler.html",
                       O_CREAT | O_WRONLY, SPOILPREFIX);
    if (fd < 0) {
        pline(msgc_debug,
              "Failed to write artifact spoiler.  Is it writable?");
        return;
    }
    if (change_fd_lock(fd, FALSE, LT_WRITE, 10)) {
        struct artifact *art;
        outfile = fdopen(fd, "w");
        fprintf(outfile, "%s", htmlheader("Artifacts"));
        fprintf(outfile, "\n<p>For weapon artifacts, see also the "
                "<a href=\"weapon-spoiler.html\">weapon spoiler</a></p>\n");
        fprintf(outfile, "\n<table id=\"artifacts\"><thead>\n  <tr>"
                "<th>name</th>"
                "<th>base object</th>"
                "<th>alignments</th>"
                /* "<th>attack</th>" */ /* Meh, see weapons spoiler. */
                "<th>when equipped</th>"
                "<th>when carried</th>"
                "<th>when invoked</th>"
                "<th>other info</th>"
                "<th class=\"numeric price\">cost</th>"
                "</tr>\n</thead><tbody>\n");

        for (art = artilist + 1; art->otyp; art++) {
            fprintf(outfile, "<tr><td class=\"artifact\">%s</td>"
                    "<td class=\"object\">%s</td>"
                    "<td class=\"artalign\">%s</td>"
                    /* "<td class=\"artattack\">%s</td>" */
                    "<td class=\"artequipped\">%s</td>"
                    "<td class=\"artcarried\">%s</td>"
                    "<td class=\"artinvoked\">%s</td>"
                    "<td class=\"artotherinfo\">%s</td>"
                    "<td class=\"numeric price\">%ld</td>"
                    "</tr>",
                    art->name, simple_typename(art->otyp),
                    spoilartalign(art),
                    /* spoilarteffects(art, 0, art->attk) */
                    spoilarteffects(art, art->spfx, art->defn),
                    spoilarteffects(art, art->cspfx, art->cary),
                    spoilartinvoke(art),
                    spoilartotherinfo(art),
                    art->cost);
        }

        fprintf(outfile, "\n</tbody></table>\n</html>\n");

        change_fd_lock(fd, FALSE, LT_NONE, 0);
        fclose(outfile);
    }    

    /* ####################### Other Objects ####################### */
    fd = open_datafile("objects-spoiler.html",
                       O_CREAT | O_WRONLY, SPOILPREFIX);
    if (fd < 0) {
        pline(msgc_debug, "Failed to write object spoiler.  Is it writable?");
        return;
    }
    if (change_fd_lock(fd, FALSE, LT_WRITE, 10)) {
        outfile = fdopen(fd, "w");
        fprintf(outfile, "%s", htmlheader("Objects"));

        fprintf(outfile, "<p>Note: for random-appearance objects, "
                "material follows appearance rather than function.</p>");

        fprintf(outfile, "<p>See also: <a href=\"weapon-spoiler.html\">"
                "Weapons Spoiler</a></p>");
        fprintf(outfile, "<p>See also: <a href=\"armor-spoiler.html\">"
                "Armor Spoiler</a></p>");
        fprintf(outfile, "<p>See also: <a href=\"artifact-spoiler.html\">"
                "Artifact Spoiler</a></p>");

        fprintf(outfile, "<p>Jump to: %s %s %s %s %s %s %s %s %s</p>",
                "<a href=\"#jewelry\">=</a>",
                msgprintf("<a href=\"#obj%d\">&quot;</a>", AMULET_OF_ESP),
                "<a href=\"#potions\">!</a>",
                "<a href=\"#scrolls\">?</a>",
                "<a href=\"#books\">+</a>",
                "<a href=\"#wands\">/</a>",
                "<a href=\"#tools\">(</a>",
                "<a href=\"#food\">%</a>",
                "<a href=\"#rocks\">*</a>");
        spoilobjclass(outfile, "Jewelry", "jewelry", RING_CLASS, AMULET_CLASS);
        /* TODO: alchemy spoiler */
        spoilobjclass(outfile, "Potions", "potions", POTION_CLASS, POTION_CLASS);
        spoilobjclass(outfile, "Scrolls", "scrolls", SCROLL_CLASS, SCROLL_CLASS);
        spoilobjclass(outfile, "Books", "books", SPBOOK_CLASS, SPBOOK_CLASS);
        spoilobjclass(outfile, "Wands", "wands", WAND_CLASS, WAND_CLASS);
        spoilobjclass(outfile, "Tools", "tools", TOOL_CLASS, TOOL_CLASS);
        spoilobjclass(outfile, "Comestibles", "food", FOOD_CLASS, FOOD_CLASS);
        fprintf(outfile, "<p>The nutritional properties of various corpses, and "
                "what resistances they can grant, are included in the "
                "<a href=\"monster-spoiler.html\">monster spoiler</a>.</p>");
        spoilobjclass(outfile, "Rocks and Gems", "rocks", GEM_CLASS, ROCK_CLASS);

        change_fd_lock(fd, FALSE, LT_NONE, 0);
        fclose(outfile);
    }

    /* ######################## Monsters ######################## */
    fd = open_datafile("monster-spoiler.html",
                       O_CREAT | O_WRONLY, SPOILPREFIX);
    
    if (fd < 0) {
        pline(msgc_debug, "Failed to write monster spoiler.  Is it writable?");
        return;
    }
    if (change_fd_lock(fd, FALSE, LT_WRITE, 10)) {
        char lastmlet = 0;
        outfile = fdopen(fd, "w");
        fprintf(outfile, "%s", htmlheader("Monsters"));
        /* navbar at top */
        fprintf(outfile, "<div class=\"nav\">Jump to: ");
        for (i = 0; mons[i].mlet; i++)
            if ((mons[i].mlet != lastmlet) && i <= PM_ARCHEOLOGIST) {
                fprintf(outfile, "<a href=\"#monst%d\">%c</a> ",
                        i, def_monsyms[(int)mons[i].mlet]);
                lastmlet = mons[i].mlet;
            }
        fprintf(outfile, "</div>");
        /* then the actual monster table */
        lastmlet = mons[0].mlet;
        headrow = "<tr><th class=\"mlet\"></th>"
            "<th class=\"monster\">monster</th>"
            "<th class=\"numeric level\">lv</th>"
            "<th class=\"numeric monstr\">mon<br />str</th>"
            "<th class=\"numeric speed\">mov</th>"
            "<th class=\"numeric ac\">def</th>"
            "<th class=\"numeric monmr willpower\">will</th>"
            "<th class=\"align\">aln</th>"
            "<th><span class=\"skills\">skills</span>"
            "    <span class=\"attacks\">attacks</span></th>"
            "<th class=\"resistances\">resists</th>"
            "<th class=\"resgranted\">grants</th>"
            "<th class=\"numeric nutrition\">nut</th>"
            "<th class=\"numeric weight\">wt</th>"
            "<th class=\"size\">sz</th>"
            "<th class=\"mrace\">race</th>"
            "<th class=\"flags\">flags</th>"
            "</tr>\n";
        fprintf(outfile, "\n<table id=\"monsters\"><thead>\n  "
                "%s</thead><tbody>\n", headrow);

        for (i = 0; mons[i].mlet; i++) {
            const boolean ul = mons[i].mcolor & HI_ULINE ? TRUE : FALSE;
            const uchar  clr = ul ? (mons[i].mcolor - HI_ULINE) :
                mons[i].mcolor;
/*
            const uchar clr  = (mons[i].mcolor > CLR_MAX) ?
                (mons[i].mcolor & CLR_MAX) : mons[i].mcolor;
*/
            const char *mlet = msgprintf("<span class=\"color%d\">"
                                         "%s%c%s</span>", clr,
                                         (ul ? "<u>" : ""),
                                         (def_monsyms[(int)mons[i].mlet]),
                                         (ul ? "</u>" : ""));
            if (i && !(i % 17)) { /* 17 plus the 1 we're adding makes 18 table
                                     rows, a multiple of three, so the headrows
                                     all get the same highlighting if we use the
                                     CSS to backlight every third row. */
                fprintf(outfile, "%s", headrow);
                lastmlet = mons[i].mlet;
            }
            fprintf(outfile, "<tr><td id=\"monst%d\" class=\"mlet\">%s</td>"
                    "<td class=\"monster\">%s</td>"
                    "<td class=\"numeric level\">%d</td>"
                    "<td class=\"numeric monstr\">%d</td>"
                    "<td class=\"numeric speed\">%d</td>"
                    "<td class=\"numeric ac\">%d</td>"
                    "<td class=\"numeric monmr\">%d</td>"
                    "<td class=\"align\">%s</td>"
                    "<td><span class=\"skills\">%s</span>"
                    "    <span class=\"attacks\">%s</span></td>"
                    "<td class=\"resistances\">%s</td>"
                    "<td class=\"resgranted\">%s</td>"
                    "<td class=\"numeric nutrition\">%d</td>"
                    "<td class=\"numeric weight\">%d</td>"
                    "<td class=\"size\">%s</td>"
                    "<td class=\"mrace\">%s</td>"
                    "<td class=\"flags\">%s</td>"
                    "</tr>\n", i, mlet, mons[i].mname, mons[i].mlevel,
                    MONSTR(i), mons[i].mmove, (10 - mons[i].ac),
                    mons[i].mr, spoilmaligntyp(i),
                    spoilmonskills(i), spoilattacks(i),
                    spoilresistances(mons[i].mresists, FALSE, i),
                    spoilresistances(mons[i].mconveys, TRUE, i),
                    mons[i].cnutrit, mons[i].cwt, spoilmonsize(mons[i].msize),
                    spoilmrace(i), spoilmonflags(i));
        }
        fprintf(outfile, "\n</tbody></table>\n</html>\n");

        change_fd_lock(fd, FALSE, LT_NONE, 0);
        fclose(outfile);     
    }
    /* ####################### Role / Race ####################### */
    fd = open_datafile("players.html",
                       O_CREAT | O_WRONLY, SPOILPREFIX);
    if (fd < 0) {
        pline(msgc_debug, "Failed to write players spoiler.  Is it writable?");
        return;
    }
    if (change_fd_lock(fd, FALSE, LT_WRITE, 10)) {
        outfile = fdopen(fd, "w");
        fprintf(outfile, "%s", htmlheader("Playable Characters"));

        fprintf(outfile, "<ul>"
                "   <li><a href=\"#race\">Race</a></li>"
                "   <li><a href=\"#role\">Role</a> (Profession/Class)</li>"
                "</ul><hr />\n");

        fprintf(outfile, "<h1><a name=\"race\">Playable Races</h1>\n");
        fprintf(outfile, "<table class=\"races\"><thead>\n"
                "  <tr><th rowspan=\"2\" class=\"filecode\">TLA</th>\n"
                "      <th class=\"player race\">Race</th>"
                "      <th class=\"size\">Size</th>"
                "      <th class=\"numeric speed\">speed</th>\n"
                "      <th class=\"gender\">Gender</th>\n"
                "      <th rowspan=\"2\" class=\"attr\">Attributes</th>\n"
                "      <th rowspan=\"2\" class=\"advance\">Advance</th></tr>\n"
                "  <tr><th class=\"player roles\" colspan=\"3\">Roles</th>\n"
                "      <th class=\"align\">Aligns</th></tr>\n"
                "</thead><tbody>\n");

        for (i = 0; races[i].filecode; i++) {
            fprintf(outfile, "<tr class=\"newsection\">"
                    "    <th rowspan=\"3\" class=\"filecode\">%s</th>"
                    "    <td class=\"player race\">%s</td>"
                    "    <td class=\"size\">%s</td>"
                    "    <td class=\"numeric speed\">%d</td>\n"
                    "    <td class=\"gender\">%s</td>\n"
                    "    <td class=\"attr\" rowspan=\"2\">%s</td>\n"
                    "    <td class=\"advance\" rowspan=\"2\">%s</td></tr>\n"
                    "<tr><td class=\"player roles\" colspan=\"3\">%s</td>\n"
                    "    <td class=\"align\">%s</td></tr>\n"
                    "<tr><td class=\"notes\" colspan=\"6\">%s</td></tr>\n",
                    races[i].filecode, races[i].noun,
                    spoilmonsize(mons[(races[i].malenum ? races[i].malenum :
                                       races[i].femalenum)].msize),
                    races[i].basespeed, spoilgenders(races[i].allow),
                    spoilattributes("min", races[i].attrmin,
                                    "max", races[i].attrmax, "", NULL),
                    spoiladvance("HP", &races[i].hpadv, "Pw", &races[i].enadv, 0),
                    spoilraceroles(races[i].selfmask), spoilaligns(races[i].allow),
                    spoilracenotes(i));
        }
        fprintf(outfile, "</tbody></table>\n");

        fprintf(outfile, "<h1><a name=\"role\">Player Roles</h1>\n");
        fprintf(outfile, "<table class=\"roles\"><thead>\n"
                "  <tr><th rowspan=\"3\" class=\"filecode\">TLA</th>\n"
                "      <th class=\"player role\">Role</th>\n"
                "      <th class=\"questart\">Artifact</th>\n"
                "      <th rowspan=\"3\" class=\"attr\">Attributes</th>\n"
                "      <th rowspan=\"3\" class=\"advance\">Advance</th>\n"
                "  </tr>\n"
                "  <tr><th class=\"race\">Races</th>\n"
                "      <th class=\"spellcasting\">Spell Penalties</th></tr>\n"
                "  <tr><th><div class=\"gender\">Gend</div>\n"
                "          <div class=\"align\">Align</div></th>\n"
                "      <th class=\"notes\">Notes</th>\n"
                "      </tr>"
                "</thead><tbody>");
        for (i = 0; roles[i].filecode; i++) {
            fprintf(outfile, "  <tr class=\"newsection\">"
                    "      <th rowspan=\"3\" class=\"filecode\">%s</th>\n"
                    "      <td class=\"player role\">%s%s%s</td>\n"
                    "      <td class=\"questart\">%s</td>\n"
                    "      <td rowspan=\"3\" class=\"attr\">%s</td>\n"
                    "      <td rowspan=\"3\" class=\"advance\">%s</td>\n"
                    "  </tr>\n"
                    "  <tr><td class=\"races\">%s</td>\n"
                    "      <td class=\"spellcasting\">%s</td></tr>\n"
                    "  <tr><td><div class=\"gender\">%s</div>\n"
                    "          <div class=\"align\">%s</div></td>\n"
                    "      <td class=\"notes\">%s</td></tr>",
                    roles[i].filecode,
                    roles[i].name.m, (roles[i].name.f ? "/" : ""),
                                    (roles[i].name.f ? roles[i].name.f : ""),
                    spoilquestart(i),
                    spoilattributes("base", roles[i].attrbase,
                                    "dist", roles[i].attrdist,
                                    "max",  roles[i].attrmaxm),
                    spoiladvance("HP", &roles[i].hpadv, "Pw", &roles[i].enadv,
                                 roles[i].xlev),
                    spoilroleraces(roles[i].allow),
                    spoilrolespellcasting(i),
                    spoilgenders(roles[i].allow),
                    spoilaligns(roles[i].allow),
                    spoilrolenotes(i));
        }
        fprintf(outfile, "</tbody></table>\n");

        change_fd_lock(fd, FALSE, LT_NONE, 0);
        fclose(outfile);
    }

    /* ######################### Index ########################### */
    fd = open_datafile("index.html",
                       O_CREAT | O_WRONLY, SPOILPREFIX);
    
    if (fd < 0) {
        pline(msgc_debug, "Failed to write spoiler index.  Is it writable?");
        return;
    }
    if (change_fd_lock(fd, FALSE, LT_WRITE, 10)) {
        outfile = fdopen(fd, "w");
        fprintf(outfile, "%s", htmlheader(""));

        fprintf(outfile, "<ul>\n"
                "   <li><a href=\"objects-spoiler.html\">Objects</a><ul>"
                "       <li><a href=\"weapon-spoiler.html\">Weapons</a></li>"
                "       <li><a href=\"armor-spoiler.html\">Armor</a></li>"
                "       <li><a href=\"objects-spoiler.html#jewelry\">Jewelry</a></li>"
                "       <li><a href=\"objects-spoiler.html#potions\">Potions</a></li>"
                "       <li><a href=\"objects-spoiler.html#scrolls\">Scrolls</a></li>"
                "       <li><a href=\"objects-spoiler.html#books\">Books &amp; Spells</a></li>"
                "       <li><a href=\"objects-spoiler.html#wands\">Wands</a></li>"
                "       <li><a href=\"objects-spoiler.html#tools\">Tools</a></li>"
                "       <li><a href=\"objects-spoiler.html#food\">Comestibles</a></li>"
                "       <li><a href=\"objects-spoiler.html#rocks\">Rocks &amp; Gems</a></li>"
                "   </ul></li>"
                "   <li><a href=\"artifact-spoiler.html\">Artifacts</a></li>"
                "   <li><a href=\"monster-spoiler.html\">Monsters</a></li>"
                "   <li><a href=\"players.html\">Players</a><ul>"
                "          <li><a href=\"players.html#race\">Races</a></li>"
                "          <li><a href=\"players.html#role\">Roles</a></li>"
                "       </ul></li>"
                "</ul>\n");

        fprintf(outfile, "\n</html>\n");
        change_fd_lock(fd, FALSE, LT_NONE, 0);
        fclose(outfile);             
    }
    
    pline(msgc_debug, "Spoiler HTML files generated.");
}

void
makepinobotyaml(void)
{
    FILE *f;
    int i, j;
    const char *variant  = "Fourk";
    const char *prefix   = "4k";
    const char *filename = msgprintf("%s_%s_%s.yaml",
                                     "Pinobot", prefix, variant);
    int fd = open_datafile(filename, O_CREAT | O_WRONLY, SPOILPREFIX);
    
    if (fd < 0) {
        pline(msgc_debug,
              "Failed to write monster .yaml for Pinobot.  Is it writeable?");
        return;
    }

    if (change_fd_lock(fd, FALSE, LT_WRITE, 10)) {
        f = fdopen(fd, "w");

        fprintf(f, "variant: \"%s\"\n", variant);
        fprintf(f, "prefix: \"%s\"\n\n", prefix);
        fprintf(f, "monsters:\n");

        for (i = 0; mons[i].mlet; i++) {
            const boolean ul = mons[i].mcolor & HI_ULINE ? TRUE : FALSE;
            const uchar  clr = ul ? (mons[i].mcolor - HI_ULINE) :
                mons[i].mcolor;
            const struct permonst *pm = &mons[i];
            struct monst dummymonst;
            memset(&dummymonst, 0, sizeof(dummymonst));
            dummymonst.data = pm;

            if (i > 0)
                fprintf(f, "\n");
            fprintf(f, " - name: \"%s\"\n", pm->mname);
            fprintf(f, "   symbol: \"%c\"\n", def_monsyms[(int)pm->mlet]);
            fprintf(f, "   base-level: %d\n", pm->mlevel);
            fprintf(f, "   difficulty: %d\n", MONSTR(i));
            fprintf(f, "   speed: %d\n", pm->mmove);
            fprintf(f, "   ac: %d\n", pm->ac);
            fprintf(f, "   mr: %d\n", pm->mr);
            fprintf(f, "   alignment: %d\n", pm->maligntyp);
            fprintf(f, "   generates:\n");
            if (pm->geno & G_UNIQ)
                fprintf(f, "    - unique\n");
            else if (pm->geno & G_NOGEN)
                fprintf(f, "%s", "");
            else {
                if ( (pm->geno & G_HELL || !(pm->geno & G_NOHELL)) )
                    fprintf(f, "    - gehennom\n");
                if ( !(pm->geno & G_HELL) )
                    fprintf(f, "    - dungeons\n");
            }
            fprintf(f, "   leaves-corpse: %s\n",
                    (pm->geno & G_NOCORPSE) ? "No" : "Yes");
            fprintf(f, "   not-generated-normally: %s\n",
                    (pm->geno & G_NOGEN) ? "Yes" : "No");
            fprintf(f, "   appears-in-small-groups: %s\n",
                    (pm->geno & G_SGROUP) ? "Yes" : "No");
            fprintf(f, "   appears-in-large-groups: %s\n",
                    (pm->geno & G_LGROUP) ? "Yes" : "No");
            fprintf(f, "   genocidable: %s\n",
                    (pm->geno & G_GENO) ? "Yes" : "No");
            fprintf(f, "   attacks: [");
            for (j = 0; j < NATTK && (pm->mattk[j].aatyp ||
                                      pm->mattk[j].adtyp ||
                                      pm->mattk[j].damn ||
                                      pm->mattk[j].damd); ++j) {
                if (j > 0)
                    fprintf(f, ", ");
                fprintf(f, "[");
                switch (pm->mattk[j].aatyp) {
                case AT_NONE: fprintf(f, "%s", "AtNone"); break;
                case AT_CLAW: fprintf(f, "%s", "AtClaw"); break;
                case AT_BITE: fprintf(f, "%s", "AtBite"); break;
                case AT_KICK: fprintf(f, "%s", "AtKick"); break;
                case AT_BUTT: fprintf(f, "%s", "AtButt"); break;
                case AT_TUCH: fprintf(f, "%s", "AtTouch"); break;
                case AT_STNG: fprintf(f, "%s", "AtSting"); break;
                case AT_HUGS: fprintf(f, "%s", "AtHug"); break;
                case AT_SPIT: fprintf(f, "%s", "AtSpit"); break;
                case AT_ENGL: fprintf(f, "%s", "AtEngulf"); break;
                case AT_BREA: fprintf(f, "%s", "AtBreath"); break;
                case AT_EXPL: fprintf(f, "%s", "AtExplode"); break;
                case AT_BOOM: fprintf(f, "%s", "AtSuicideExplode"); break;
                case AT_GAZE: fprintf(f, "%s", "AtGaze"); break;
                case AT_TENT: fprintf(f, "%s", "AtTentacle"); break;
                case AT_SPIN: fprintf(f, "%s", "AtSpin"); break;
                case AT_WEAP: fprintf(f, "%s", "AtWeapon"); break;
                case AT_MAGC: fprintf(f, "%s", "AtCast"); break;
                default:      fprintf(f, "%s", "AtUnknown");
                    pline(msgc_debug, "Error: Unknown attack type: %d",
                          pm->mattk[j].aatyp);
                    break;
                }
                switch (pm->mattk[j].adtyp) {
                case AD_PHYS: fprintf(f, ", %s", "AdPhys"); break;
                case AD_MAGM: fprintf(f, ", %s", "AdMagicMissile"); break;
                case AD_FIRE: fprintf(f, ", %s", "AdFire"); break;
                case AD_COLD: fprintf(f, ", %s", "AdCold"); break;
                case AD_SLEE: fprintf(f, ", %s", "AdSleep"); break;
                case AD_DISN: fprintf(f, ", %s", "AdDisintegrate"); break;
                case AD_ELEC: fprintf(f, ", %s", "AdElectricity"); break;
                case AD_DRST: fprintf(f, ", %s", "AdStrDrain"); break;
                case AD_ACID: fprintf(f, ", %s", "AdAcid"); break;
                case AD_SPC1: fprintf(f, ", %s", "AdSpc1");
                    pline(msgc_debug, "Warning: AD_SPC1 used directly"); break;
                case AD_SPC2: fprintf(f, ", %s", "AdSpc2");
                    pline(msgc_debug, "Warning: AD_SPC2 used directly"); break;
                case AD_BLND: fprintf(f, ", %s", "AdBlind"); break;
                case AD_STUN: fprintf(f, ", %s", "AdStun"); break;
                case AD_SLOW: fprintf(f, ", %s", "AdSlow"); break;
                case AD_PLYS: fprintf(f, ", %s", "AdParalyse"); break;
                case AD_DRLI: fprintf(f, ", %s", "AdLevelDrain"); break;
                case AD_DREN: fprintf(f, ", %s", "AdMagicDrain"); break;
                case AD_LEGS: fprintf(f, ", %s", "AdLegs"); break;
                case AD_STON: fprintf(f, ", %s", "AdStone"); break;
                case AD_STCK: fprintf(f, ", %s", "AdSticking"); break;
                case AD_SGLD: fprintf(f, ", %s", "AdGoldSteal"); break;
                case AD_SITM: fprintf(f, ", %s", "AdItemSteal"); break;
                case AD_SEDU: fprintf(f, ", %s", "AdSeduce"); break;
                case AD_TLPT: fprintf(f, ", %s", "AdTeleport"); break;
                case AD_RUST: fprintf(f, ", %s", "AdRust"); break;
                case AD_CONF: fprintf(f, ", %s", "AdConfuse"); break;
                case AD_DGST: fprintf(f, ", %s", "AdDigest"); break;
                case AD_HEAL: fprintf(f, ", %s", "AdHeal"); break;
                case AD_WRAP: fprintf(f, ", %s", "AdWrap"); break;
                case AD_WERE: fprintf(f, ", %s", "AdWere"); break;
                case AD_DRDX: fprintf(f, ", %s", "AdDexDrain"); break;
                case AD_DRCO: fprintf(f, ", %s", "AdConDrain"); break;
                case AD_DRIN: fprintf(f, ", %s", "AdIntDrain"); break;
                case AD_DISE: fprintf(f, ", %s", "AdDisease"); break;
                case AD_DCAY: fprintf(f, ", %s", "AdRot"); break;
                case AD_SSEX: fprintf(f, ", %s", "AdSex"); break;
                case AD_HALU: fprintf(f, ", %s", "AdHallucination"); break;
                case AD_DETH: fprintf(f, ", %s", "AdDeath"); break;
                case AD_PEST: fprintf(f, ", %s", "AdPestilence"); break;
                case AD_FAMN: fprintf(f, ", %s", "AdFamine"); break;
                case AD_SLIM: fprintf(f, ", %s", "AdSlime"); break;
                case AD_ENCH: fprintf(f, ", %s", "AdDisenchant"); break;
                case AD_CORR: fprintf(f, ", %s", "AdCorrode"); break;
                case AD_FLPN: fprintf(f, ", %s", "AdFeelPain"); break;
                case AD_SCLD: fprintf(f, ", %s", "AdStinkingCloud"); break;
                case AD_PITS: fprintf(f, ", %s", "AdPits"); break;
                case AD_ICEB: fprintf(f, ", %s", "AdIceBlock"); break;
                case AD_WEBS: fprintf(f, ", %s", "AdWebs"); break;
                case AD_DISP: fprintf(f, ", %s", "AdDisplace"); break;
                case AD_CLRC: fprintf(f, ", %s", "AdClerical"); break;
                case AD_SPEL: fprintf(f, ", %s", "AdSpell"); break;
                case AD_RBRE: fprintf(f, ", %s", "AdRandomBreath"); break;
                case AD_SAMU: fprintf(f, ", %s", "AdAmuletSteal"); break;
                case AD_CURS: fprintf(f, ", %s", "AdCurse"); break;
                default:      fprintf(f, ", %s", "AdUnknown");
                    pline(msgc_debug, "Error: Unknown damage type: %d",
                          pm->mattk[j].adtyp);
                    break;
                }
                fprintf(f, ", %d, %d]", pm->mattk[j].damn, pm->mattk[j].damd);
            }
            fprintf(f, "]\n");
            {
                boolean comma_set = FALSE;
                fprintf(f, "   skills: [");
#define SKILL(a, b) j = ((pm->mskill / a) % 4);                         \
                if ( comma_set ) fprintf(f, ", ");                      \
                if (j >= 1) {                                           \
                    comma_set = TRUE;                                   \
                    fprintf(f, "[%s, %s]", (b),                         \
                            ((j >= 3) ? "Expert" :                      \
                             (j >= 2) ? "Skilled" : "Basic")); }
                SKILL(MP_WANDS, "SkWand")
#undef SKILL
                    fprintf(f, "]\n");
            }
            fprintf(f, "   weight: %d\n", pm->cwt);
            fprintf(f, "   nutrition: %d\n", pm->cnutrit);
            fprintf(f, "   size: ");
            if (pm->msize == MZ_TINY) fprintf(f, "tiny\n");
            else if (pm->msize == MZ_SMALL) fprintf(f, "small\n");
            else if (pm->msize == MZ_MEDIUM) fprintf(f, "medium\n");
            else if (pm->msize == MZ_LARGE) fprintf(f, "large\n");
            else if (pm->msize == MZ_HUGE) fprintf(f, "huge\n");
            else if (pm->msize == MZ_GIGANTIC) fprintf(f, "gigantic\n");
            else {
                fprintf(f, "unknownsize\n");
                pline(msgc_debug, "Error: Unknown size: %d", pm->msize);
            }

            fprintf(f, "   resistances:\n");
            if (pm->mresists & MR_FIRE) fprintf(f, "    - ReFire\n");
            if (pm->mresists & MR_COLD) fprintf(f, "    - ReCold\n");
            if (pm->mresists & MR_SLEEP) fprintf(f, "    - ReSleep\n");
            if (pm->mresists & MR_DISINT) fprintf(f, "    - ReDisintegrate\n");
            if (pm->mresists & MR_ELEC) fprintf(f, "    - ReElectricity\n");
            if (pm->mresists & MR_POISON) fprintf(f, "    - RePoison\n");
            if (pm->mresists & MR_ACID) fprintf(f, "    - ReAcid\n");
            if (pm->mresists & MR_STONE) fprintf(f, "    - RePetrification\n");
            if (resists_magm(&dummymonst)) fprintf(f, "    - ReMagic\n");
            if (resists_drli(&dummymonst)) fprintf(f, "    - ReDrain\n");
            
            fprintf(f, "   conferred:\n");
            if (pm->mconveys & MR_FIRE) fprintf(f, "    - ReFire\n");
            if (pm->mconveys & MR_COLD) fprintf(f, "    - ReCold\n");
            if (pm->mconveys & MR_SLEEP) fprintf(f, "    - ReSleep\n");
            if (pm->mconveys & MR_DISINT) fprintf(f, "    - ReDisintegrate\n");
            if (pm->mconveys & MR_ELEC) fprintf(f, "    - ReElectricity\n");
            if (pm->mconveys & MR_POISON) fprintf(f, "    - RePoison\n");
            if (pm->mconveys & MR_ACID) fprintf(f, "    - ReAcid\n");
            /* You can't actually get petrification resistance this way. */
            // if (pm->mconveys & MR_STONE) fprintf(f, "    - RePetrification\n");
        
            fprintf(f, "   flags: [");
            {
                int comma_set = 0;
#define AT(a, b) if (pm->mflags1 & a) {                 \
                    if ( comma_set ) fprintf(f, ", ");  \
                    comma_set = 1;                      \
                    fprintf(f, "%s", b); }
                AT(M1_FLY, "FlFly");
                AT(M1_SWIM, "FlSwim");
                AT(M1_AMORPHOUS, "FlAmorphous");
                AT(M1_WALLWALK, "FlWallwalk");
                AT(M1_CLING, "FlCling");
                AT(M1_TUNNEL, "FlTunnel");
                AT(M1_NEEDPICK, "FlNeedPick");
                AT(M1_CONCEAL, "FlConceal");
                AT(M1_HIDE, "FlHide");
                AT(M1_AMPHIBIOUS, "FlAmphibious");
                AT(M1_BREATHLESS, "FlBreathless");
                AT(M1_NOTAKE, "FlNoTake");
                AT(M1_NOEYES, "FlNoEyes");
                AT(M1_NOHANDS, "FlNoHands");
                AT(M1_NOLIMBS, "FlNoLimbs");
                AT(M1_NOHEAD, "FlNoHead");
                AT(M1_MINDLESS, "FlMindless");
                AT(M1_HUMANOID, "FlHumanoid");
                AT(M1_ANIMAL, "FlAnimal");
                AT(M1_SLITHY, "FlSlithy");
                AT(M1_UNSOLID, "FlUnSolid");
                AT(M1_THICK_HIDE, "FlThickHide");
                AT(M1_OVIPAROUS, "FlOviparous");
                AT(M1_REGEN, "FlRegen");
                AT(M1_SEE_INVIS, "FlSeeInvis");
                AT(M1_TPORT, "FlTeleport");
                AT(M1_TPORT_CNTRL, "FlTeleportControl");
                AT(M1_ACID, "FlAcid");
                AT(M1_POIS, "FlPoisonous");
                AT(M1_CARNIVORE, "FlCarnivore");
                AT(M1_HERBIVORE, "FlHerbivore");
                AT(M1_METALLIVORE, "FlMetallivore");
#undef AT
#define AT(a, b) if (pm->mflagsr & a) {                 \
                    if ( comma_set ) fprintf(f, ", ");  \
                    comma_set = 1;                      \
                    fprintf(f, "%s", b); }
                AT(MRACE_HUMAN, "FlHuman");
                AT(MRACE_ELF, "FlElf");
                AT(MRACE_DWARF, "FlDwarf");
                AT(MRACE_GNOME, "FlGnome");
                AT(MRACE_ORC, "FlOrc");
                AT(MRACE_FAIRY, "FlFairy");
#undef AT
#define AT(a, b) if (pm->mflags2 & a) {                 \
                    if ( comma_set ) fprintf(f, ", ");  \
                    comma_set = 1;                      \
                    fprintf(f, "%s", b); }
                AT(M2_NOPOLY, "FlNoPoly");
                AT(M2_UNDEAD, "FlUndead");
                AT(M2_WERE, "FlWere");
                AT(M2_DEMON, "FlDemon");
                AT(M2_MERC, "FlMerc");
                AT(M2_LORD, "FlLord");
                AT(M2_PRINCE, "FlPrince");
                AT(M2_MINION, "FlMinion");
                AT(M2_GIANT, "FlGiant");
                AT(M2_MALE, "FlMale");
                AT(M2_FEMALE, "FlFemale");
                AT(M2_NEUTER, "FlNeuter");
                AT(M2_PNAME, "FlProperName");
                AT(M2_HOSTILE, "FlHostile");
                AT(M2_PEACEFUL, "FlPeaceful");
                AT(M2_DOMESTIC, "FlDomestic");
                AT(M2_WANDER, "FlWander");
                AT(M2_STALK, "FlStalk");
                AT(M2_NASTY, "FlNasty");
                AT(M2_STRONG, "FlStrong");
                AT(M2_ROCKTHROW, "FlRockThrow");
                AT(M2_GREEDY, "FlGreedy");
                AT(M2_JEWELS, "FlJewels");
                AT(M2_COLLECT, "FlCollect");
                AT(M2_MAGIC, "FlMagicCollect");
#undef AT
                if (passes_walls(pm)) {
                    fprintf(f, ", FlPhasing");
                }
#define AT(a, b) if (pm->mflags3 & a) {                 \
                    if ( comma_set ) fprintf(f, ", ");  \
                    comma_set = 1;                      \
                    fprintf(f, "%s", b); }
                AT(M3_WANTSAMUL, "FlWantsAmulet");
                AT(M3_WANTSBELL, "FlWantsBell");
                AT(M3_WANTSBOOK, "FlWantsBook");
                AT(M3_WANTSCAND, "FlWantsCand");
                AT(M3_WANTSARTI, "FlWantsArti");
                AT(M3_WANTSALL, "FlWantsAll");
                AT(M3_WAITFORU, "FlWaitsForYou");
                AT(M3_CLOSE, "FlClose");
                AT(M3_COVETOUS, "FlCovetous");
                AT(M3_INFRAVISIBLE, "FlInfravisible");
                AT(M3_INFRAVISION, "FlInfravision");
                AT(M3_SCENT, "FlScentTracker");
                AT(M3_DISPLACES, "FlDisplaces");
                AT(M3_BLINKAWAY, "FlBlinkAway");
                AT(M3_VANDMGRDUC, "FlVanDmgRduc");
#undef AT
                if (hates_material(pm, SILVER)) fprintf(f, ", FlHatesSilver");
                if (passes_bars(pm)) fprintf(f, ", FlPassesBars");
                if (vegan(pm)) fprintf(f, ", FlVegan");
                else if (vegetarian(pm)) fprintf(f, ", FlVegetarian");
            }
            fprintf(f, "]\n");
            fprintf(f, "   color: ");
            
            if (ul)
                fprintf(f, "Underlined ");
            switch(clr) {
            case CLR_BLACK: fprintf(f, "Black"); break;
            case CLR_RED: fprintf(f, "Red"); break;
            case CLR_GREEN: fprintf(f, "Green"); break;
            case CLR_BROWN: fprintf(f, "Brown"); break;
            case CLR_BLUE: fprintf(f, "Blue"); break;
            case CLR_MAGENTA: fprintf(f, "Magenta"); break;
            case CLR_CYAN: fprintf(f, "Cyan"); break;
            case CLR_GRAY: fprintf(f, "Gray"); break;
            case CLR_ORANGE: fprintf(f, "Orange"); break;
            case CLR_BRIGHT_GREEN: fprintf(f, "BrightGreen"); break;
            case CLR_BRIGHT_BLUE: fprintf(f, "BrightBlue"); break;
            case CLR_BRIGHT_CYAN: fprintf(f, "BrightCyan"); break;
            case CLR_BRIGHT_MAGENTA: fprintf(f, "BrightMagenta"); break;
            case CLR_YELLOW: fprintf(f, "Yellow"); break;
            case CLR_WHITE: fprintf(f, "White"); break;
            default: pline(msgc_debug,
                           "Error: I don't know what color %d is.\n", clr);
                fprintf(f, "UnknownColor"); break;
            }
            fprintf(f, "\n");
            
        }

        fprintf(f, "all-monster-names: [");
        for (i = 0; mons[i].mname[0]; ++i)
        {
            if (i > 0)
                fprintf(f, ", ");
            fprintf(f, "\"%s\"", mons[i].mname);
        }
        fprintf(f, "]\n\n");

        change_fd_lock(fd, FALSE, LT_NONE, 0);
        fclose(f);
        pline(msgc_debug, "YAML for Pinobot generated: %s", filename);
    }
}

void
makespoilers(void)
{
    makehtmlspoilers();
    makepinobotyaml();
}
