#!/usr/bin/env python3
"""Generate fr/de/it translation files from en.json + a curated value->translation
table. Translates by ENGLISH VALUE (not symbol), so every symbol sharing a value
is covered, and only clearly-translatable UI labels are touched — names, country
codes, and competition names fall through to English (tr() returns the compiled
string when a key is absent, so untranslated strings just render in English).

SWOS is uppercase-only; translations use UPPERCASE with real accents (decoded to
Latin-1 by translations.c tr_dup). Extend TR[...] freely — re-run to regenerate.

  python3 scripts/gen_translations.py     (writes bin/data_json/translations/{fr,de,it}.json)
"""
import json, os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TDIR = os.path.join(REPO, "bin", "data_json", "translations")
LANGS = ("fr", "de", "it", "es", "nl")

# english value -> (fr, de, it). UPPERCASE, accented. Keep only confident UI terms.
TR = {
 # --- buttons / actions ---
 "ABORT":        ("ABANDONNER", "ABBRECHEN",   "ANNULLA"),
 "ACCEPT":       ("ACCEPTER",   "ANNEHMEN",    "ACCETTA"),
 "ACCEPT OFFER": ("ACCEPTER L'OFFRE", "ANGEBOT ANNEHMEN", "ACCETTA OFFERTA"),
 "CANCEL":       ("ANNULER",    "ABBRECHEN",   "ANNULLA"),
 "CONTINUE":     ("CONTINUER",  "WEITER",      "CONTINUA"),
 "EXIT":         ("QUITTER",    "BEENDEN",     "ESCI"),
 "YES":          ("OUI",        "JA",          "SÌ"),
 "NO":           ("NON",        "NEIN",        "NO"),
 "OK":           ("OK",         "OK",          "OK"),
 "BACK":         ("RETOUR",     "ZURÜCK",      "INDIETRO"),
 "RETURN":       ("RETOUR",     "ZURÜCK",      "INDIETRO"),
 "NEXT":         ("SUIVANT",    "NÄCHSTE",     "PROSSIMO"),
 "SAVE":         ("SAUVER",     "SPEICHERN",   "SALVA"),
 "LOAD":         ("CHARGER",    "LADEN",       "CARICA"),
 "QUIT":         ("QUITTER",    "BEENDEN",     "ESCI"),
 "DONE":         ("TERMINÉ",    "FERTIG",      "FATTO"),
 "ASK FOR MORE": ("DEMANDER PLUS", "MEHR VERLANGEN", "CHIEDI DI PIÙ"),
 # --- menus ---
 "OPTIONS":      ("OPTIONS",    "OPTIONEN",    "OPZIONI"),
 "CAREER":       ("CARRIÈRE",   "KARRIERE",    "CARRIERA"),
 "FRIENDLY":     ("AMICAL",     "FREUNDSCHAFT","AMICHEVOLE"),
 "PLAY MATCH":   ("JOUER LE MATCH", "SPIEL SPIELEN", "GIOCA PARTITA"),
 "NEW GAME":     ("NOUVELLE PARTIE", "NEUES SPIEL", "NUOVA PARTITA"),
 "LOAD GAME":    ("CHARGER PARTIE",  "SPIEL LADEN", "CARICA PARTITA"),
 "EDIT TEAMS":   ("MODIFIER ÉQUIPES","TEAMS BEARBEITEN","MODIFICA SQUADRE"),
 "CHOOSE TEAM":  ("CHOISIR ÉQUIPE",  "TEAM WÄHLEN", "SCEGLI SQUADRA"),
 "CHOOSE TEAMS": ("CHOISIR ÉQUIPES", "TEAMS WÄHLEN","SCEGLI SQUADRE"),
 "CHOOSE SEASON":("CHOISIR SAISON",  "SAISON WÄHLEN","SCEGLI STAGIONE"),
 "CHANGE SQUAD": ("CHANGER EFFECTIF","KADER ÄNDERN", "CAMBIA ROSA"),
 "CHANGE TEAMS": ("CHANGER ÉQUIPES", "TEAMS ÄNDERN","CAMBIA SQUADRE"),
 # --- match ---
 "GOAL":         ("BUT",        "TOR",         "GOL"),
 "HALF TIME":    ("MI-TEMPS",   "HALBZEIT",    "INTERVALLO"),
 "FULL TIME":    ("FIN DU MATCH","SPIELENDE",  "FINE PARTITA"),
 "KICK OFF":     ("COUP D'ENVOI","ANSTOSS",    "CALCIO D'INIZIO"),
 "ATTACK":       ("ATTAQUE",    "ANGRIFF",     "ATTACCO"),
 "DEFENCE":      ("DÉFENSE",    "ABWEHR",      "DIFESA"),
 "MIDFIELD":     ("MILIEU",     "MITTELFELD",  "CENTROCAMPO"),
 "GOALKEEPER":   ("GARDIEN",    "TORWART",     "PORTIERE"),
 "DEFENDER":     ("DÉFENSEUR",  "ABWEHR",      "DIFENSORE"),
 "ATTACKER":     ("ATTAQUANT",  "STÜRMER",     "ATTACCANTE"),
 "SUBSTITUTE":   ("REMPLAÇANT", "AUSWECHSLUNG","SOSTITUTO"),
 "PENALTY":      ("PÉNALTY",    "ELFMETER",    "RIGORE"),
 "BOOKINGS":     ("CARTONS",    "VERWARNUNGEN","AMMONIZIONI"),
 "INJURY":       ("BLESSURE",   "VERLETZUNG",  "INFORTUNIO"),
 "RESULT":       ("RÉSULTAT",   "ERGEBNIS",    "RISULTATO"),
 "ALL MATCHES":  ("TOUS LES MATCHS","ALLE SPIELE","TUTTE LE PARTITE"),
 # --- career / club ---
 "COACH":        ("ENTRAÎNEUR", "TRAINER",     "ALLENATORE"),
 "PLAYER":       ("JOUEUR",     "SPIELER",     "GIOCATORE"),
 "TEAM":         ("ÉQUIPE",     "TEAM",        "SQUADRA"),
 "CLUB TEAMS":   ("ÉQUIPES DE CLUB","VEREINSTEAMS","SQUADRE DI CLUB"),
 "BANK":         ("BANQUE",     "BANK",        "BANCA"),
 "BANK STATEMENT":("RELEVÉ BANCAIRE","KONTOAUSZUG","ESTRATTO CONTO"),
 "BUY PLAYER":   ("ACHETER JOUEUR","SPIELER KAUFEN","COMPRA GIOCATORE"),
 "SELL PLAYER":  ("VENDRE JOUEUR", "SPIELER VERKAUFEN","VENDI GIOCATORE"),
 "ADD PLAYER":   ("AJOUTER JOUEUR","SPIELER HINZU","AGGIUNGI GIOCATORE"),
 "SEASON":       ("SAISON",     "SAISON",      "STAGIONE"),
 "DIVISION":     ("DIVISION",   "LIGA",        "DIVISIONE"),
 "CLUBS":        ("CLUBS",      "VEREINE",     "CLUB"),
 "CONFIRM OFFER":("CONFIRMER L'OFFRE","ANGEBOT BESTÄTIGEN","CONFERMA OFFERTA"),
 "APPROX VALUE": ("VALEUR APPROX.","CA. WERT",  "VALORE APPROX."),
 "BEST REGARDS": ("CORDIALEMENT","MIT FREUNDLICHEN GRÜSSEN","CORDIALI SALUTI"),
 "COMPUTER":     ("ORDINATEUR", "COMPUTER",    "COMPUTER"),
 # --- common labels ---
 "NAME":         ("NOM",        "NAME",        "NOME"),
 "AGE":          ("ÂGE",        "ALTER",       "ETÀ"),
 "VALUE":        ("VALEUR",     "WERT",        "VALORE"),
 "SCORE":        ("SCORE",      "STAND",       "PUNTEGGIO"),
 "POSITION":     ("POSITION",   "POSITION",    "POSIZIONE"),
 # --- months (3-letter, accented) ---
 "JAN":("JAN","JAN","GEN"), "FEB":("FÉV","FEB","FEB"), "MAR":("MAR","MÄR","MAR"),
 "APR":("AVR","APR","APR"), "MAY":("MAI","MAI","MAG"), "JUN":("JUN","JUN","GIU"),
 "JUL":("JUL","JUL","LUG"), "AUG":("AOÛ","AUG","AGO"), "SEP":("SEP","SEP","SET"),
 "OCT":("OCT","OKT","OTT"), "NOV":("NOV","NOV","NOV"), "DEC":("DÉC","DEZ","DIC"),
}

# Spanish kept separate so the TR tuples stay 3-wide. english value -> es (UPPERCASE,
# accented: Á É Í Ó Ú Ñ ¿ ¡ all exist in the font extension).
ES = {
 "ABORT":"ABORTAR", "ACCEPT":"ACEPTAR", "ACCEPT OFFER":"ACEPTAR OFERTA",
 "CANCEL":"CANCELAR", "CONTINUE":"CONTINUAR", "EXIT":"SALIR", "YES":"SÍ", "NO":"NO",
 "OK":"OK", "BACK":"ATRÁS", "RETURN":"VOLVER", "NEXT":"SIGUIENTE", "SAVE":"GUARDAR",
 "LOAD":"CARGAR", "QUIT":"SALIR", "DONE":"HECHO", "ASK FOR MORE":"PEDIR MÁS",
 "OPTIONS":"OPCIONES", "CAREER":"CARRERA", "FRIENDLY":"AMISTOSO",
 "PLAY MATCH":"JUGAR PARTIDO", "NEW GAME":"NUEVO JUEGO", "LOAD GAME":"CARGAR JUEGO",
 "EDIT TEAMS":"EDITAR EQUIPOS", "CHOOSE TEAM":"ELEGIR EQUIPO",
 "CHOOSE TEAMS":"ELEGIR EQUIPOS", "CHOOSE SEASON":"ELEGIR TEMPORADA",
 "CHANGE SQUAD":"CAMBIAR PLANTILLA", "CHANGE TEAMS":"CAMBIAR EQUIPOS",
 "GOAL":"GOL", "HALF TIME":"DESCANSO", "FULL TIME":"FINAL", "KICK OFF":"SAQUE INICIAL",
 "ATTACK":"ATAQUE", "DEFENCE":"DEFENSA", "MIDFIELD":"MEDIOCAMPO",
 "GOALKEEPER":"PORTERO", "DEFENDER":"DEFENSA", "ATTACKER":"DELANTERO",
 "SUBSTITUTE":"SUPLENTE", "PENALTY":"PENALTI", "BOOKINGS":"TARJETAS",
 "INJURY":"LESIÓN", "RESULT":"RESULTADO", "ALL MATCHES":"TODOS LOS PARTIDOS",
 "COACH":"ENTRENADOR", "PLAYER":"JUGADOR", "TEAM":"EQUIPO",
 "CLUB TEAMS":"EQUIPOS DE CLUB", "BANK":"BANCO", "BANK STATEMENT":"EXTRACTO BANCARIO",
 "BUY PLAYER":"COMPRAR JUGADOR", "SELL PLAYER":"VENDER JUGADOR",
 "ADD PLAYER":"AÑADIR JUGADOR", "SEASON":"TEMPORADA", "DIVISION":"DIVISIÓN",
 "CLUBS":"CLUBES", "CONFIRM OFFER":"CONFIRMAR OFERTA", "APPROX VALUE":"VALOR APROX.",
 "BEST REGARDS":"UN CORDIAL SALUDO", "COMPUTER":"ORDENADOR", "NAME":"NOMBRE",
 "AGE":"EDAD", "VALUE":"VALOR", "SCORE":"MARCADOR", "POSITION":"POSICIÓN",
 "JAN":"ENE","FEB":"FEB","MAR":"MAR","APR":"ABR","MAY":"MAY","JUN":"JUN",
 "JUL":"JUL","AUG":"AGO","SEP":"SEP","OCT":"OCT","NOV":"NOV","DEC":"DIC",
}

# Dutch — also a separate dict. Mostly plain ASCII; CARRIÈRE uses È (covered).
NL = {
 "ABORT":"AFBREKEN", "ACCEPT":"ACCEPTEREN", "ACCEPT OFFER":"BOD ACCEPTEREN",
 "CANCEL":"ANNULEREN", "CONTINUE":"DOORGAAN", "EXIT":"AFSLUITEN", "YES":"JA", "NO":"NEE",
 "OK":"OK", "BACK":"TERUG", "RETURN":"TERUG", "NEXT":"VOLGENDE", "SAVE":"OPSLAAN",
 "LOAD":"LADEN", "QUIT":"AFSLUITEN", "DONE":"KLAAR", "ASK FOR MORE":"VRAAG MEER",
 "OPTIONS":"OPTIES", "CAREER":"CARRIÈRE", "FRIENDLY":"VRIENDSCHAPPELIJK",
 "PLAY MATCH":"SPEEL WEDSTRIJD", "NEW GAME":"NIEUW SPEL", "LOAD GAME":"SPEL LADEN",
 "EDIT TEAMS":"TEAMS BEWERKEN", "CHOOSE TEAM":"KIES TEAM", "CHOOSE TEAMS":"KIES TEAMS",
 "CHOOSE SEASON":"KIES SEIZOEN", "CHANGE SQUAD":"WIJZIG SELECTIE",
 "CHANGE TEAMS":"WIJZIG TEAMS", "GOAL":"DOELPUNT", "HALF TIME":"RUST", "FULL TIME":"EINDE",
 "KICK OFF":"AFTRAP", "ATTACK":"AANVAL", "DEFENCE":"VERDEDIGING", "MIDFIELD":"MIDDENVELD",
 "GOALKEEPER":"KEEPER", "DEFENDER":"VERDEDIGER", "ATTACKER":"AANVALLER",
 "SUBSTITUTE":"WISSEL", "PENALTY":"STRAFSCHOP", "BOOKINGS":"KAARTEN", "INJURY":"BLESSURE",
 "RESULT":"RESULTAAT", "ALL MATCHES":"ALLE WEDSTRIJDEN", "COACH":"TRAINER",
 "PLAYER":"SPELER", "TEAM":"TEAM", "CLUB TEAMS":"CLUBTEAMS", "BANK":"BANK",
 "BANK STATEMENT":"BANKAFSCHRIFT", "BUY PLAYER":"SPELER KOPEN",
 "SELL PLAYER":"SPELER VERKOPEN", "ADD PLAYER":"SPELER TOEVOEGEN", "SEASON":"SEIZOEN",
 "DIVISION":"DIVISIE", "CLUBS":"CLUBS", "CONFIRM OFFER":"BOD BEVESTIGEN",
 "APPROX VALUE":"CA. WAARDE", "BEST REGARDS":"MET VRIENDELIJKE GROET",
 "COMPUTER":"COMPUTER", "NAME":"NAAM", "AGE":"LEEFTIJD", "VALUE":"WAARDE",
 "SCORE":"STAND", "POSITION":"POSITIE",
 "JAN":"JAN","FEB":"FEB","MAR":"MRT","APR":"APR","MAY":"MEI","JUN":"JUN",
 "JUL":"JUL","AUG":"AUG","SEP":"SEP","OCT":"OKT","NOV":"NOV","DEC":"DEC",
}

# Languages with their own value->translation dict (the rest use the TR 3-tuples).
SEP = {"es": ES, "nl": NL}
TR_IDX = {"fr": 0, "de": 1, "it": 2}

# Bulk machine-translation table (english value -> {lang: text}), built by
# merge_agents() from the per-language translator outputs. The small curated
# TR/ES/NL dicts above OVERRIDE it (hand-authored wins); anything not in either
# falls back to English. Edit translations_master.json to fix a bulk translation.
MASTER = os.path.join(REPO, "scripts", "translations_master.json")
MASTER_DATA = json.load(open(MASTER, encoding="utf-8")) if os.path.exists(MASTER) else {}

def lookup(v, lang):
    if lang in SEP and v in SEP[lang]:          # curated override (es/nl)
        return SEP[lang][v]
    if lang in TR_IDX and v in TR:              # curated override (fr/de/it)
        return TR[v][TR_IDX[lang]]
    m = MASTER_DATA.get(v)                       # bulk machine translation
    if m and lang in m:
        return m[lang]
    return v                                     # English fallback

def merge_agents():
    """Fold /tmp/tr_<lang>.json translator outputs into translations_master.json,
    keeping only entries that actually changed (value != key)."""
    master = dict(MASTER_DATA)
    for lang in LANGS:
        p = "/tmp/tr_%s.json" % lang
        if not os.path.exists(p):
            print("  missing", p); continue
        d = json.load(open(p, encoding="utf-8"))
        n = 0
        for eng, tr in d.items():
            if isinstance(tr, str) and tr.strip() and tr != eng:
                master.setdefault(eng, {})[lang] = tr; n += 1
        print("  %s: +%d translations" % (lang, n))
    json.dump(master, open(MASTER, "w", encoding="utf-8"),
              ensure_ascii=False, sort_keys=True, indent=0)
    print("wrote %s: %d english values" % (MASTER, len(master)))

def main():
    en = json.load(open(os.path.join(TDIR, "en.json"), encoding="utf-8"))
    for lang in LANGS:
        strings = {k: lookup(v, lang) for k, v in en["strings"].items()}
        out = {"language": lang, "strings": strings, "countries": en["countries"]}
        path = os.path.join(TDIR, lang + ".json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(out, f, ensure_ascii=False, indent=1)
            f.write("\n")
        n = sum(1 for v in en["strings"].values() if lookup(v, lang) != v)
        print("wrote %s.json: %d translated, %d English fallback" % (lang, n, len(strings) - n))

if __name__ == "__main__":
    import sys
    if "--merge" in sys.argv:
        merge_agents()
        MASTER_DATA = json.load(open(MASTER, encoding="utf-8"))   # reload for main()
    main()
