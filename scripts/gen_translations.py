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
LANGS = ("fr", "de", "it", "es")

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

def lookup(v, lang, i):
    if lang == "es":
        return ES.get(v, v)
    return TR[v][i] if v in TR else v

def main():
    en = json.load(open(os.path.join(TDIR, "en.json"), encoding="utf-8"))
    for i, lang in enumerate(LANGS):
        table = ES if lang == "es" else TR
        strings = {k: lookup(v, lang, i) for k, v in en["strings"].items()}
        out = {"language": lang, "strings": strings, "countries": en["countries"]}
        path = os.path.join(TDIR, lang + ".json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(out, f, ensure_ascii=False, indent=1)
            f.write("\n")
        n = sum(1 for v in en["strings"].values() if v in table)
        print(f"wrote {lang}.json: {n} strings translated, {len(strings)-n} English fallback")

if __name__ == "__main__":
    main()
