#!/usr/bin/env python3
"""
Genera il PDF della conversazione sul progetto big-font LCD.
I blocchi di codice sono sostituiti con il marcatore <CODICE>.
"""

from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import cm
from reportlab.lib import colors
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, HRFlowable, KeepTogether, Image as RLImage
)
from reportlab.lib.enums import TA_LEFT, TA_CENTER, TA_JUSTIFY

# ---------------------------------------------------------------------------
# Stili
# ---------------------------------------------------------------------------
BASE = getSampleStyleSheet()

TITLE = ParagraphStyle(
    "Title",
    parent=BASE["Heading1"],
    fontSize=20,
    leading=26,
    spaceAfter=6,
    alignment=TA_CENTER,
    textColor=colors.HexColor("#1a1a2e"),
)
SUBTITLE = ParagraphStyle(
    "Subtitle",
    parent=BASE["Normal"],
    fontSize=10,
    leading=14,
    spaceAfter=20,
    alignment=TA_CENTER,
    textColor=colors.HexColor("#555555"),
)
SECTION = ParagraphStyle(
    "Section",
    parent=BASE["Heading2"],
    fontSize=13,
    leading=18,
    spaceBefore=16,
    spaceAfter=4,
    textColor=colors.HexColor("#16213e"),
)
USER = ParagraphStyle(
    "User",
    parent=BASE["Normal"],
    fontSize=10,
    leading=14,
    spaceBefore=10,
    spaceAfter=2,
    leftIndent=0,
    textColor=colors.HexColor("#0f3460"),
    fontName="Helvetica-Bold",
)
ASSISTANT = ParagraphStyle(
    "Assistant",
    parent=BASE["Normal"],
    fontSize=10,
    leading=15,
    spaceBefore=2,
    spaceAfter=8,
    leftIndent=12,
    textColor=colors.HexColor("#222222"),
    fontName="Helvetica",
    alignment=TA_JUSTIFY,
)
CODE_MARKER = ParagraphStyle(
    "Code",
    parent=BASE["Normal"],
    fontSize=9,
    leading=13,
    spaceBefore=4,
    spaceAfter=4,
    leftIndent=24,
    textColor=colors.HexColor("#888888"),
    fontName="Courier",
)
NOTE = ParagraphStyle(
    "Note",
    parent=BASE["Normal"],
    fontSize=9,
    leading=13,
    spaceBefore=2,
    spaceAfter=6,
    leftIndent=12,
    textColor=colors.HexColor("#444444"),
    fontName="Helvetica-Oblique",
    alignment=TA_JUSTIFY,
)

def u(text):
    return [Paragraph("Utente:", USER), Paragraph(text, ASSISTANT)]

def a(text):
    return [Paragraph("Assistente:", USER), Paragraph(text, ASSISTANT)]

def codice():
    return [Paragraph("&lt;CODICE&gt;", CODE_MARKER)]

def hr():
    return [Spacer(1, 4), HRFlowable(width="100%", thickness=0.5,
            color=colors.HexColor("#cccccc")), Spacer(1, 4)]

def note(text):
    return [Paragraph(f"<i>{text}</i>", NOTE)]

def section(text):
    return [Paragraph(text, SECTION)]

# ---------------------------------------------------------------------------
# Contenuto della conversazione
# ---------------------------------------------------------------------------
story = []

# Titolo
story += [
    Spacer(1, 1*cm),
    Paragraph("Progetto Big-Font LCD", TITLE),
    Paragraph(
        "Trascrizione della conversazione · Febbraio–Aprile 2026",
        SUBTITLE,
    ),
]
story += hr()

# ── 1 ── IDEA INIZIALE ──────────────────────────────────────────────────────
story += section("1 — Idea iniziale")
story += u(
    "Vorrei disegnare dei caratteri. Il modulo base è formato da 5×8 pixel. "
    "Ciascun carattere può essere formato da 4 moduli base (quindi 10×16) oppure "
    "da due moduli base (quindi 5×16). Per formare i moduli base ho solo 8 griglie "
    "disponibili. Cioè occorre trovare 8 griglie 5×8 che usate in vario modo "
    "riescano a generare i numeri da 0 a 9."
)
story += a(
    "Il problema è ben posto: un display LCD HD44780 offre esattamente 8 slot CGRAM "
    "per caratteri personalizzati 5×8. L'idea è costruire le dieci cifre come "
    "mosaici di 2×2 tile (10×16 pixel) o 1×2 tile (5×16 pixel), riutilizzando "
    "gli stessi 8 pattern con eventuali specchiature orizzontali. "
    "Ho progettato i seguenti tile base: A (barra superiore + vert. sinistra), "
    "C (vert. sinistra + barra inferiore), F (barra superiore + barra centrale, "
    "simmetrico), G (barra superiore + vert. destra + barra centrale), "
    "M (solo barra inferiore). Le varianti flip(A), flip(C), flip(G) si ottengono "
    "specchiando ogni riga. Il carattere SPACE (0x20) della ROM del display viene "
    "usato come modulo vuoto senza consumare slot CGRAM."
)

# ── 2 ── VISUALIZZAZIONE PNG ────────────────────────────────────────────────
story += section("2 — Visualizzazione PNG con Python")
story += u(
    "Mi interessa vedere come verrebbero visualizzati. Usa gli strumenti che vuoi "
    "(ma meglio Python in venv). Alla fine vorrei 10 PNG in cui si intravedono le "
    "griglie sotto."
)
story += a(
    "Ho creato il file requirements.txt (dipendenza: Pillow) e lo script "
    "generate.py. Il sistema di rendering usa queste costanti principali: "
    "PIXEL_SCALE=10 (pixel PNG per pixel logico), THIN_LINE=1 (gap tra pixel "
    "adiacenti), THICK_LINE=3 (gap ai bordi modulo), PADDING=20 (margine esterno). "
    "La palette è ambra su sfondo quasi-nero (LED display look). "
    "Ogni modulo ha un colore di sfondo leggermente diverso (tint) per rendere "
    "visibile la composizione."
)
story += codice()

# ── 3 ── CONTEGGIO MODULI ───────────────────────────────────────────────────
story += section("3 — Conteggio dei moduli distinti")
story += u("In totale quanti moduli 5×8 differenti hai usato?")
story += a(
    "Contando anche le varianti specchiate e il tile blank usato per il digit 1, "
    "risultavano 10 pattern distinti, troppi per i soli 8 slot CGRAM disponibili. "
    "Era necessario ridurre."
)

# ── 4 ── RIDUZIONE A 8 MODULI + CARTELLA MODULI ─────────────────────────────
story += section("4 — Riduzione a esattamente 8 moduli")
story += u("Occorre ridurre a 8. Produce in una cartella dal nome 'moduli' tutti i singoli moduli 5×8.")
story += a(
    "Per ridurre a 8 ho eliminato i tile ridondanti: il tile H era pixel-per-pixel "
    "identico a C; il tile J (sola barra superiore) e il tile N (blank) erano usati "
    "solo per il digit 7 e il digit 1 nella versione narrow. Ridisegnando quei "
    "due digit (7 con barra trasversale europea, 1 nella variante wide) ho potuto "
    "rimuoverli. I 8 tile definitivi sono: A, flip(A), C, flip(C), F, G, flip(G), M. "
    "Per i moduli individuali ho creato la cartella moduli/ con un PNG 136×199 px "
    "per ciascuno dei 8 custom, con etichetta del nome nell'angolo."
)
story += codice()

# ── 5 ── DIGIT 3 E 7 IDENTICI — PDF DATASHEET ───────────────────────────────
story += section("5 — Problema: digit 3 e 7 identici")
story += u(
    "Il problema è che 3 e 7 sono identici. Tieni conto che puoi utilizzare in "
    "modo creativo anche i caratteri standard del display. Li trovi nel PDF che "
    "ho copiato nella cartella del progetto."
)
story += a(
    "Ho letto il datasheet del display Powertip PC1602ARS-CWA-A-Q (compatibile "
    "HD44780). La pagina 13 mostra la ROM dei caratteri standard. "
    "Il problema era che la prima versione del digit 7 con barra trasversale "
    "(TL=F, TR=G) era strutturalmente identica al digit 3. "
    "La soluzione: digit 7 ridisegnato con TL=A, TR=flip(A) → arco in cima "
    "(come la metà superiore del digit 0) + solo il tratto destro che scende "
    "(BL=SPACE ROM, BR=flip(C)). Così 7 è visivamente molto diverso da 3 "
    "(TL=F, TR=G, BL=M, BR=flip(C))."
)

# ── 6 ── TRATTI VERTICALI TROPPO SOTTILI ────────────────────────────────────
story += section("6 — Miglioramento dei tratti verticali")
story += u("Il problema è che 1 e 7 sono i meno riusciti. Si possono migliorare?")
story += a(
    "Il problema era che i tratti verticali erano larghi solo 2 pixel, risultando "
    "troppo esili. Ho portato tutti i tratti verticali a 3 pixel: nelle definizioni "
    "dei tile A, C e G ogni riga del tratto verticale è passata da '##...' a "
    "'###..' (e simmetricamente per le varianti flip). Questo ha reso le cifre "
    "più robuste e leggibili. Contestualmente, il digit 7 con BL=SPACE (carattere "
    "ROM del display) è stato formalizzato: SPACE non occupa uno slot CGRAM e "
    "viene scritto con il codice 0x20."
)

# ── 7 ── CARATTERE VUOTO PER DIGIT 7 ────────────────────────────────────────
story += section("7 — Uso del carattere spazio ROM per il digit 7")
story += u("Suggerisci nel carattere 7 di usare il carattere vuoto per il modulo in basso a sinistra.")
story += a(
    "Esatto: il modulo BL del digit 7 è completamente vuoto, quindi usare il "
    "carattere spazio della ROM (0x20) è la scelta corretta — non spreca nessuno "
    "degli 8 slot CGRAM custom. Ho aggiunto SPACE al dizionario TILES (tutte "
    "righe di punti), escluso da THE_8_MODULES, e gestito in resolve_tile(). "
    "Il tint di sfondo per SPACE è (0,0,0) — il nero puro del display."
)

# ── 8 ── RIMOZIONE ETICHETTE + REDESIGN DIGIT 1 ─────────────────────────────
story += section("8 — Rimozione etichette dalle cifre e redesign del digit 1")
story += u(
    "Togliere anche il carattere in alto a sinistra nella cifra. "
    "Provare a fare la cifra uno usando i moduli A e flip(C) e lasciare la parte "
    "sinistra vuota."
)
story += a(
    "Ho rimosso la chiamata a draw_tile_labels() dalla funzione render_digit() "
    "(le etichette restano solo nei PNG della cartella moduli/). "
    "Il digit 1 è stato ridisegnato in formato wide (10×16): "
    "TL=SPACE, TR=A, BL=SPACE, BR=flip(C). "
    "La metà sinistra è completamente vuota, mentre quella destra porta il "
    "tratto verticale con barra superiore (A) e il tratto verticale con barra "
    "inferiore (flip(C)). L'effetto è un digit 1 stretto che sfrutta solo la "
    "metà destra del suo spazio nominale."
)

# ── 9 ── ARDUINO SKETCH BIGFONT ─────────────────────────────────────────────
story += section("9 — Sketch Arduino: bigfont.ino")
story += u(
    "Attacco all'Arduino: sulla porta seriale c'è un Arduino Nano con un display "
    "2 righe da 20 caratteri. Proveresti a scrivere 0123456789? Nel file cablaggio "
    "trovi come è collegato."
)
story += a(
    "Ho letto il file cablaggio.txt: RS→D12, R/W→D11 (tenuto LOW), E→D10, "
    "D4→D5, D5→D4, D6→D3, D7→D2. "
    "Ho creato lo sketch bigfont/bigfont.ino che carica gli 8 tile nei slot "
    "CGRAM 0–7 e scrive tutte le 10 cifre grandi affiancate sulle 20 colonne. "
    "Ogni cifra occupa 2 colonne × 2 righe di caratteri LCD standard (5×8). "
    "I byte CGRAM sono codificati con i 5 LSB per riga (bit4 = colonna sinistra). "
    "Il pin D11 viene impostato LOW via pinMode/digitalWrite prima di "
    "lcd.begin(). Il cast esplicito (byte) in lcd.write() per lo slot 0 evita "
    "l'ambiguità con il terminatore di stringa."
)
story += codice()
story += note(
    "Compilato con arduino-cli (FQBN arduino:avr:nano): 2002 byte flash (6%), "
    "149 byte RAM (7%). Caricato su /dev/cu.usbserial-1140."
)

# ── 10 ── SIMULAZIONE OROLOGIO ───────────────────────────────────────────────
story += section("10 — Simulazione orologio a 4 cifre")
story += u(
    "Prova a simulare un orologio a 4 cifre contando i moduli: per le ore 2 per "
    "la prima cifra, 1 modulo di separazione e 2 per la seconda cifra. Stessa cosa "
    "per i minuti. 3 moduli vuoti fra il blocco delle ore e quello dei minuti."
)
story += a(
    "Ho calcolato il layout: i blocchi ore (5 col) + gap centrale (3 col) + "
    "blocco minuti (5 col) = 13 colonne totali, centrate sulle 20 del display "
    "(margine sx 3, dx 4). "
    "Il layout definitivo per colonna: H1 a col 3-4, separatore col 5, H2 a "
    "col 6-7, gap col 8-10 (con ':' lampeggiante al centro in col 9), M1 a col "
    "11-12, separatore col 13, M2 a col 14-15. "
    "Lo sketch clock/clock.ino parte da 00:00, avanza con millis() "
    "(TICK_MS=1000 per tempo reale, abbassabile per simulazione rapida), "
    "ridisegna solo le cifre cambiate per evitare flickering, e fa lampeggiare "
    "il ':' ogni secondo."
)
story += codice()
story += note(
    "Compilato: 2642 byte flash (8%), 169 byte RAM (8%). Caricato con successo."
)

# ── 11 ── ORA DI PARTENZA DA SERIALE ────────────────────────────────────────
story += section("11 — Ora di partenza via porta seriale")
story += u(
    "Per prova meglio accettare dalla seriale un numero a 4 cifre come partenza."
)
story += a(
    "Ho aggiunto la gestione seriale (9600 baud) allo sketch clock.ino. "
    "All'avvio il display mostra il messaggio di attesa ('Ora iniziale: / HHMM + invio') "
    "finché non arriva un input valido. "
    "Il parser accumula i caratteri cifra in un buffer di 4 byte e processa "
    "l'input al ricevimento del newline (\\n); il carriage return (\\r) viene "
    "ignorato per compatibilità Windows. Validazione: ore 00-23, minuti 00-59. "
    "In caso di errore risponde sulla seriale ('ERR: …'). "
    "È possibile inviare una nuova ora mentre l'orologio gira per fare reset "
    "istantaneo, senza riavviare l'Arduino."
)
story += codice()
story += note(
    "Compilato: 4428 byte flash (14%), 506 byte RAM (24%). Caricato con successo."
)

# ── 12 ── DIGIT 4 VUOTO IN BASSO SX + SERIALE STARTUP + ORA FISSA ───────────
story += section("12 — Digit 4 senza modulo in basso a sinistra; avvio automatico")
story += u(
    "Nel carattere 4 lascerei vuoto il modulo in basso a sinistra. "
    "All'accensione sulla seriale scrivi il nome dello sketch, quando è stato "
    "creato e quando è stato compilato (1 riga per ciascuna cosa). "
    "Poi aggiungi una riga con scritto 'ghedo 2026 >>> run' con delle righe vuote "
    "a capo. Poi pubblica il messaggio per chiedere l'ora di inizio. "
    "Non display all'accensione, parti con l'ora 07:29 senza scrivere nient'altro."
)
story += a(
    "Tre file modificati in contemporanea. "
    "Nel DIGIT_MAP il digit 4 passa da BL=M a BL=SPACE: il modulo in basso a "
    "sinistra diventa completamente vuoto, accentuando la forma del 4 con solo "
    "il tratto destro che scende nella metà inferiore. Il PNG digit_4.png è stato "
    "rigenerato. "
    "In clock.ino il setup() ora stampa sulla seriale, nell'ordine: "
    "il nome dello sketch ('Sketch : clock'), la data di creazione "
    "('Creato : 21 Feb 2026'), la data/ora di compilazione usando le macro "
    "del preprocessore C (__DATE__ e __TIME__). Segue una riga vuota, "
    "poi '--- ghedo 2026 >>> run ---' con righe vuote prima e dopo, "
    "e infine il prompt 'Ora di inizio? HHMM + invio'. "
    "Il display LCD si avvia immediatamente con 07:29 senza mostrare alcun "
    "messaggio di attesa: startClock(7,29) viene chiamato direttamente in setup(). "
    "La gestione seriale rimane attiva nel loop() per consentire il reset "
    "dell'ora in qualsiasi momento."
)
story += codice()
story += note(
    "Compilato: 4678 byte flash (15%), 608 byte RAM (29%). Caricato con successo."
)

# ── 13 ── DIGIT 1: BR RIDISEGNATO CON CARATTERE ROM 0x16 ─────────────────────
story += section("13 — Digit 1: modulo BR sostituito con il carattere ROM 0x16")
story += u(
    "Prova a ridisegnare la cifra 1 usando il carattere predefinito nella ROM "
    "del display 0x16."
)
story += a(
    "Il carattere ROM 0x16 della Futaba M202SD16FA è una barra verticale piena "
    "delle 3 colonne di destra ('..###' ripetuto su tutte le 8 righe), senza "
    "alcuna barra orizzontale. Confrontato con il precedente BR=C (che portava "
    "'###..' nelle righe 0-5 e '#####' nelle righe 6-7), il carattere 0x16 "
    "allinea perfettamente il tratto verticale inferiore con quello superiore "
    "proveniente da flip(A) ('..###' righe 2-7). "
    "Il digit 1 diventa: TL=SPACE, TR=flip(A), BL=SPACE, BR=ROM_16. "
    "Contestualmente è stato corretto anche TR che nella versione precedente era "
    "A (verticale sinistra) invece di flip(A) (verticale destra). "
    "Nessuno slot CGRAM viene consumato per il modulo inferiore del digit 1. "
    "Generate.py, bigfont.ino e clock.ino aggiornati; PNG rigenerati."
)
story += note(
    "Compilato e caricato: 5148 byte flash (16%), 662 byte RAM (32%)."
)

# ── 14 ── RICONSIDERAZIONE DEL FONT: BARRE VERTICALI ROM ─────────────────────
story += section("14 — Riconsiderazione del font alla luce delle barre ROM")
story += u(
    "Riconsidera la creazione del font alla luce dei nuovi caratteri che hai "
    "trovato nella ROM (le barre verticali)."
)
story += a(
    "La ROM Futaba contiene due serie di barre verticali solide (tutte le 8 righe, "
    "nessuna barra orizzontale): 0x10-0x14 (riempimento sinistro, da 1 a 5 colonne) "
    "e 0x15-0x18 (riempimento destro, da 4 a 1 colonna). "
    "In particolare 0x12 = '###..' e 0x16 = '..###' sono le verticali pure a "
    "3 colonne, identiche alla porzione verticale dei tile flip(A) e flip(C) "
    "ma senza barre orizzontali. "
    "Analisi sistematica: i tile CGRAM restano tutti necessari perché i corner "
    "(barra+verticale combinati in una singola cella) non esistono nella ROM; "
    "le barre ROM coprono solo la verticale pura. "
    "Le uniche posizioni nel DIGIT_MAP dove una barra terminale è 'gratuita' "
    "(non strutturalmente richiesta dal carattere) sono: digit 1 BR (già ROM_16) "
    "e digit 7 BR, dove flip(C) aggiungeva un piede in basso a destra non "
    "tipograficamente necessario. "
    "Digit 7 BR cambiato da flip(C) a ROM_16: l'arco superiore (A+flip(A), "
    "identico alla metà alta del digit 0) scende ora con una verticale pulita "
    "senza piede, coerente con il digit 1. "
    "Tutti gli altri usi di flip(C) (digit 0, 3, 4, 5, 6, 8, 9) chiudono "
    "angoli strutturali e non sono sostituibili. "
    "Generate.py, bigfont.ino e clock.ino aggiornati; PNG rigenerati."
)
story += note(
    "Compilato e caricato: 5148 byte flash (16%), 662 byte RAM (32%)."
)

# ── 15 ── REDESIGN ANIMAZIONE RESPIRO ────────────────────────────────────────
story += section("15 — Redesign animazione respiro con barre ROM")
story += u(
    "Vorrei cambiare la sequenza del respiro. Consideriamo solo i primi due "
    "caratteri della prima riga. All'inizio saranno vuoti; nella fase di "
    "inspirazione il primo carattere inizia a riempirsi da destra verso sinistra, "
    "il secondo da sinistra verso destra. In questo modo alla fine del respiro "
    "saranno tutti e due pieni. Lo svuotamento sarà simmetrico fino ad arrivare "
    "a vuotarli tutti e due."
)
story += a(
    "L'animazione usa i caratteri ROM 0x10-0x18 (barre di riempimento): "
    "col 0 usa la serie destra (0x18=1col, 0x17=2col, ..., 0x14=pieno), "
    "col 1 usa la serie sinistra (0x10=1col, 0x11=2col, ..., 0x14=pieno). "
    "I due caratteri si espandono dall'intercapedine centrale verso l'esterno. "
    "La sequenza ha 13 fasi: pausa vuota (2 step), inspiro (5 step), "
    "pausa al picco (1 step), espiro (5 step). "
    "La funzione drawBreath() precedente (4 angoli, fase sfasata per riga "
    "inferiore) è stata sostituita con una che scrive solo col 0-1 riga 0."
)

story += u("Vorrei che l'andamento dell'animazione seguisse i tempi di un respiro.")
story += a(
    "Struttura fisiologica implementata: inspirazione ~1.8s (contrazione attiva, "
    "rallenta verso il picco), pausa al picco ~0.35s, espirazione ~2.7s "
    "(ritorno elastico passivo: parte veloce poi si esaurisce), pausa al vuoto "
    "~0.75s. Ciclo totale ~5.5s ≈ 11 respiri/minuto."
)

story += u("Velocizza del 30%.")
story += a(
    "Tutti i tempi moltiplicati per 0.7. Ciclo ridotto a ~3.8s ≈ 16 respiri/minuto."
)

story += u("Aumenta un po' la pausa quando i caratteri sono tutti vuoti.")
story += a("Pausa al vuoto portata da 420ms a 750ms (500+250ms).")

story += u(
    "Replica l'animazione anche negli ultimi due caratteri della prima riga "
    "(sfalsandola di un terzo rispetto a quelli della prima riga) e mettila "
    "anche negli ultimi due caratteri della seconda riga, sfasandola di 2/3 "
    "rispetto a quella dei primi due caratteri della prima riga."
)
story += a(
    "La funzione drawBreath() ora gestisce 4 gruppi di 2 caratteri (cols 0-1 e "
    "cols 18-19 su entrambe le righe). Gli sfasamenti di fase (su BREATH_LEN=13): "
    "riga 0 sx = base (0), riga 1 sx = +1/4 (+3 step), "
    "riga 0 dx = +1/3 (+4 step), riga 1 dx = +2/3 (+8 step). "
    "Ogni gruppo scrive chCol0[step] e chCol1[step] nella propria posizione."
)

story += u(
    "Lascia inalterate le pause ma velocizza del 20% le velocità di riempimento."
)
story += a(
    "I tempi di inspiro ed espiro moltiplicati per 0.8; le pause (vuota 500+250ms, "
    "picco 245ms) lasciate invariate. "
    "Inspiro: 84, 140, 196, 252, 336ms. Espiro: 140, 224, 308, 392, 448ms."
)
story += u("Velocizza anche lo svuotamento del 20%.")
story += a("Espiro ulteriormente moltiplicato per 0.8 (×0.51 rispetto all'originale): "
           "90, 143, 197, 251, 286ms.")

story += u("Allungale pause fra quando la sequenza in alto a sinistra finisce "
           "ed inizia quella in basso a sinistra.")
story += a(
    "Con BREATH_LEN=13 e offset +3, la riga 1 iniziava a riempirsi esattamente "
    "quando la riga 0 entrava nel suo ultimo step vuoto: gap zero. "
    "Soluzione: aggiunto uno step vuoto finale (step 13, 380ms), portando "
    "BREATH_LEN a 14. Con offset BREATH_LEN/4=3, quando la base è a step 13 "
    "entrambe le righe sono vuote; la riga 1 inizia a riempirsi solo al ciclo "
    "successivo (base=0), dopo 380ms di silenzio condiviso."
)

story += u("Velocizza del 20% i soli riempimenti.")
story += a("Inspiro ed espiro moltiplicati per 0.8 (×0.64 complessivo): "
           "inspiro 67, 112, 157, 202, 269ms; espiro 90, 143, 197, 251, 286ms. "
           "Pause invariate.")

story += u("Velocizza anche lo svuotamento del 20%.")
story += a("Espiro moltiplicato per ulteriore 0.8: 72, 114, 158, 201, 229ms.")

story += u("Mi sembra che vada un po' a scatti.")
story += a(
    "Causa: solo 5 livelli di riempimento ROM (1-5 colonne), ogni step è un "
    "salto di 1/5 del carattere. Soluzione: sfasamento interno di col0 e col1 "
    "di 1 livello, portando i livelli da 5 a 7 (0=vuoto, 6=pieno) e le "
    "transizioni visibili da 5 a 6 per direzione. "
    "chCol0[7] = { 0x20, 0x20, 0x18, 0x17, 0x16, 0x15, 0x14 } "
    "(col0 in ritardo: inizia tardi, finisce tardi). "
    "chCol1[7] = { 0x20, 0x10, 0x11, 0x12, 0x13, 0x14, 0x14 } "
    "(col1 in anticipo: inizia prima, finisce prima). "
    "Effetto: durante l'inspiro col1 parte un step prima di col0 creando "
    "un'onda; durante l'espiro col0 svuota prima. BREATH_LEN portato a 16 "
    "(2 pausa + 6 inspiro + 1 picco + 6 espiro + 1 finale)."
)
story += note(
    "Ultima compilazione: 5234 byte flash (17%), 676 byte RAM (33%). Caricato."
)

# ── clockMov ─────────────────────────────────────────────────────────────────
story += hr()
story += section("clockMov — sviluppi successivi")
story += a(
    "Creato il nuovo sketch clockMov/clockMov.ino derivato da clock.ino. "
    "Rimosse: animazione respiro (drawBreath, breathStep, breathDur, chCol0, chCol1) "
    "e variabili associate (breathPhase, lastBreath); lampeggio dei due punti "
    "(colonOn, drawColon con logica on/off). "
    "I due punti sono ora fissi (sempre accesi). "
    "Da questo punto tutte le modifiche riguardano esclusivamente clockMov."
)
story += note(
    "Compilato: 4642 byte flash (15%), 610 byte RAM (29%). Caricato su "
    "/dev/cu.usbserial-130."
)

story += u("Il separatore ha smesso di lampeggiare. Ripristinalo.")
story += a(
    "Aggiunto lampeggio dei due punti: variabile colonOn, drawColon(bool on) "
    "che scrive 0x96 o spazio, toggle in loop() a ogni tick, colonOn=true "
    "in startClock()."
)
story += note("Compilato: 4680 byte flash (15%). Caricato.")

story += u(
    "Per facilitare il test: se scrivo 'p' sulla seriale il tempo si incrementa "
    "di 1 minuto; se premo 'l' si incrementa di 10 minuti."
)
story += a(
    "Aggiunta funzione advanceMinutes(n): incrementa mm di n, gestisce il "
    "riporto su hh (modulo 24), azzera ss, ridisegna le cifre e aggiorna "
    "lastTick. In handleSerialChar() i caratteri 'p' e 'l' (fuori da un "
    "buffer HHMM) chiamano rispettivamente advanceMinutes(1) e advanceMinutes(10). "
    "Risposta seriale: '+1min -> HH:MM'. Il carattere freccia -> sostituisce "
    "UTF-8 per compatibilità con il monitor seriale."
)
story += note("Compilato: 6080 byte flash (19%). Caricato.")

story += u(
    "Per alcuni cambi di tempo non vedo l'animazione."
)
story += a(
    "advanceMinutes() disegnava le cifre direttamente senza animazione. "
    "La logica di animazione è stata estratta nella funzione animateDigits(h1,h2,m1,m2) "
    "condivisa da loop() e da advanceMinutes(). La funzione confronta le cifre "
    "in ingresso con last_h1/h2, last_m1/m2 per determinare quali posizioni "
    "animare; advanceMinutes() non tocca più i valori last_*, così la comparazione "
    "rileva correttamente le cifre cambiate."
)
story += note("Compilato: 6084 byte flash (19%). Caricato.")

story += u(
    "Prova un'animazione diversa: fai scappare le cifre a destra o a sinistra."
)
story += a(
    "Sostituita l'animazione verticale (top/bottom) con un wipe orizzontale a "
    "4 frame, operando su singole colonne. Frame 1: cancella colonna sinistra "
    "(vecchia esce a destra). Frame 2: cancella colonna destra (cella vuota). "
    "Frame 3: disegna colonna sinistra della nuova cifra (entra da sinistra). "
    "Frame 4: disegna colonna destra (cifra completa). ANIM_STEP=70ms, totale ~280ms. "
    "Macro WCOL() per scrivere un singolo tile a riga e colonna specificata. "
    "Le cifre che cambiano simultaneamente si animano in parallelo."
)
story += note("Compilato: 6212 byte flash (20%). Caricato.")

story += u(
    "È sbagliato: il digit 1 deve essere vicino alla cifra adiacente, "
    "non avere troppo spazio a sinistra."
)
story += a(
    "Il problema: il digit 1 occupa due colonne con TL=SPACE, quindi la sua "
    "barra visibile è sempre nella colonna destra. Dove appaia in posizione "
    "sinistra (H2, M2) il gap con il vicino era eccessivo. "
    "La soluzione adottata è position-dependent: "
    "anziché modificare la forma del digit, si spostano le posizioni in base "
    "al valore della cifra. "
    "Regole implementate: (1) se H2=1, H1 si sposta +1 colonna verso destra "
    "(l'intero blocco ore si avvicina al centro); "
    "(2) se M1=1, M1 si sposta -1 colonna verso sinistra (la barra si avvicina al ':'); "
    "(3) M2 si sposta -1 per M2=1 e un ulteriore -1 per M1=1 (mantiene il gap di 1 col con M1). "
    "Le variabili ocol_*/ncol_* tengono traccia della colonna vecchia e nuova per "
    "ciascuna cifra, così animateDigits() gestisce correttamente sia i cambi di valore "
    "che i cambi di posizione (re-draw forzato quando ncol != ocol). "
    "La funzione startClock() usa le stesse regole per il disegno iniziale."
)
story += note("Compilato: 6910 byte flash (22%), 620 byte RAM (30%). Caricato.")

story += u(
    "Premo 'l' dalle 10:50 e ottengo '1:00' invece di '11:00'."
)
story += a(
    "Bug: quando H1=1 (cambio di posizione, stesso valore) e H2=1 (cambio di valore) "
    "si animano contemporaneamente, H1 arriva alla posizione finale con la barra alla "
    "colonna 5. Poi H2 esegue il cleanup finale wSP(COL_H2-1)=wSP(5), cancellando "
    "la barra di H1. La causa radice: per il digit 1 il cleanup 'wSP(col-1)' è "
    "inutile (TL=SPACE, quindi non c'è mai nulla da pulire a sinistra) e pericoloso "
    "(può sovrascrivere la colonna destra del digit vicino). "
    "Fix: il cleanup dell'ultimo frame viene condizionato a DIGITS[d][0]!=0x20. "
    "Se il modulo TL è SPACE (solo il digit 1), il wSP viene saltato completamente."
)
story += note("Compilato: 6910 byte flash (22%), 620 byte RAM (30%). Caricato.")

# ── clockSaveDispaly ──────────────────────────────────────────────────────────
story += hr()
story += section("clockSaveDispaly — salvataggio fase")
story += u("Questa fase del progetto è finita. Salva lo sketch in un nuovo sketch dal nome clockSaveDispaly.")
story += a(
    "Creato clockSaveDispaly/clockSaveDispaly.ino come copia stabile di clockMov.ino "
    "al termine della fase di sviluppo. "
    "Aggiornati: nome sketch nell'intestazione, data di creazione (02 Mar 2026), "
    "stringa Serial.println nel setup(). "
    "Il file è identico a clockMov.ino nella logica, nei tile CGRAM, nel layout "
    "e in tutte le correzioni accumulate durante la fase (posizionamento adattivo "
    "del digit 1, fix wSP condizionato)."
)

# ── SHIFT ORIZZONTALE PER DISTRIBUZIONE COLONNE ───────────────────────────────
story += section("Shift orizzontale periodico per distribuzione colonne")
story += u(
    "Il display ha 20 colonne ma l'orologio usa solo quelle centrali. "
    "Vorrei distribuire l'uso delle colonne in modo da usarle circa allo stesso modo."
)
story += a(
    "Il layout dell'orologio occupa 13 colonne centrali (3–15), lasciando "
    "le colonne 0–2 e 16–19 sempre spente. Per distribuire il consumo, "
    "introdotto un offset orizzontale globale (colOffset) che sposta l'intero "
    "blocco dell'orologio. "
    "L'offset è calcolato come (mm % 8) - 3, quindi varia da -3 a +4 "
    "(8 posizioni) e cambia naturalmente a ogni minuto. "
    "Tutte le funzioni (animateDigits, drawColon, redrawAll) usano "
    "COL_* + colOffset. Il bounds check in wc() (col 0–19) taglia "
    "silenziosamente le animazioni ai bordi. "
    "Il digit 1 adattivo si somma correttamente all'offset globale."
)
story += note("Compilato: 7232 byte flash (23%), 622 byte RAM (30%). Caricato.")

story += u(
    "Allo scadere del minuto, prima fai la transizione della cifra "
    "(la cifra scappa fuori) e poi muovi l'orologio."
)
story += a(
    "Invertito l'ordine: prima animateDigits() (con l'offset vecchio, "
    "così le cifre escono e rientrano nella posizione corrente), "
    "poi updateOffset() che, se l'offset è cambiato, fa lcd.clear() "
    "e ridisegna tutto nella nuova posizione. "
    "L'effetto visivo è: transizione cifra → snap alla nuova posizione."
)
story += note("Compilato: 7212 byte flash (23%), 622 byte RAM (30%). Caricato.")

# ── clockMoveSepAnimazione ────────────────────────────────────────────────────
story += hr()
story += section("clockMoveSepAnimazione — salvataggio fase")
story += u("Copia lo sketch in clockMoveSepAnimazione, da adesso modificheremo solo questa nuova copia.")
story += a(
    "Creato clockMoveSepAnimazione/clockMoveSepAnimazione.ino come copia stabile "
    "di clockMov.ino al termine della fase di sviluppo dello shift orizzontale. "
    "Aggiornati: nome sketch nell'intestazione, data di creazione (07 Mar 2026), "
    "stringa Serial.println nel setup(). "
    "Il file include tutte le funzionalità accumulate: big-font, animazione "
    "orizzontale, digit 1 adattivo, shift periodico delle colonne con "
    "sequenza animazione → spostamento layout."
)

# ── ANIMAZIONE SEPARATORE ─────────────────────────────────────────────────────
story += section("Animazione separatore con losanghe ROM")
story += u(
    "Vorrei cambiare l'animazione del separatore. Forse usare i caratteri "
    "0x96 (losanga piena) e 0x97 (losanga vuota). Ho messo nella cartella "
    "un disegno dei 9 passi dell'animazione che vorrei fare."
)
story += a(
    "Dal disegno dell'utente, la sequenza a 9 passi (riga 0 = alto, riga 1 = basso): "
    "(1) vuoto/vuoto, (2) ◇ alto, (3) ◇ basso, (4) ◆ basso, "
    "(5) ◆ alto + ◇ basso, (6) ◆ alto + ◆ basso, (7) ◇ alto + ◆ basso, "
    "(8) ◆ alto + ◇ basso, (9) ◇ alto. Poi ricomincia. "
    "Il vecchio drawColon(bool) è stato sostituito con drawColon() senza parametri "
    "che legge la fase corrente dalla tabella colonAnim[9][2]. "
    "La variabile colonOn è stata rimpiazzata da colonPhase (0–8). "
    "Generato anche un PNG di preview (colon_anim_preview.png) con i 9 passi "
    "numerati, stile ambra su sfondo scuro."
)
story += note("Compilato: 7276 byte flash (23%), 656 byte RAM (32%). Caricato.")

story += u("Prova l'animazione con diverse coppie di caratteri ROM.")
story += a(
    "Testate le seguenti coppie (vuoto/pieno): "
    "0x96/0x97 (losanghe ◇/◆), "
    "0x3C/0x3E (&lt;/&gt;), "
    "0x11/0x17, "
    "0x1C/0x1F, "
    "0x8C/0x2F, "
    "0x94/0x95. "
    "Scelta finale: losanghe 0x97 (vuota) e 0x96 (piena)."
)

story += note(
    "Preview della sequenza a 9 passi generata in colon_anim_preview.png: "
    "(1) vuoto/vuoto, (2) ◇ alto, (3) ◇ basso, (4) ◆ basso, "
    "(5) ◆ alto + ◇ basso, (6) ◆ + ◆, (7) ◇ alto + ◆ basso, "
    "(8) ◆ alto + ◇ basso, (9) ◇ alto."
)
story += [Spacer(1, 6),
          RLImage("/Users/ghedo/script/AllClaude/char1/colon_anim_preview.png",
                  width=15*cm, height=15*cm * 190 / 920, kind='proportional'),
          Spacer(1, 6)]

story += u("L'animazione è troppo lenta, velocizzala.")
story += a(
    "Il timer dell'animazione separatore è stato separato dal tick dell'orologio. "
    "Nuovo timer indipendente lastColon con COLON_MS=500ms per passo "
    "(ciclo completo ~4.5s, prima era 9s con un passo per secondo). "
    "Il loop() ora gestisce due timer: uno per il separatore (500ms) "
    "e uno per l'avanzamento dell'orologio (1000ms)."
)
story += note("Compilato: 7358 byte flash (23%), 660 byte RAM (32%). Caricato.")

story += u("Fai durare il passo 1 più a lungo rispetto agli altri.")
story += a(
    "Introdotta una tabella di durate per passo (colonDur[]). "
    "Dopo vari aggiustamenti della durata del passo 1 e dei passi successivi "
    "(1000/500, 1500/300, 2000/250, 2400/200, poi passi veloci a 60ms, 0ms, 90ms)."
)

story += u("Modifica la sequenza: 1 2 3 4 5 6 7 8 9 8 7 6 5 4 3 2 (ping-pong).")
story += a(
    "Sequenza estesa a 16 frame con andamento ping-pong: i 9 passi base "
    "seguiti dal ritorno (8→2). Passo 1 a 2400ms, gli altri a 90ms."
)

story += u("Prova con 1 2 3 4 5 6 7 8 9 3 4 5 6 7 8 6 7 4 3.")
story += a(
    "Sequenza estesa a 19 frame con coda che ripete parzialmente la salita "
    "e aggiunge un rimbalzo finale. Passo 1 a 2400ms, gli altri a 90ms."
)

story += u("Durate variabili: rampa da 50ms a 100ms (picco al passo 9), poi discesa a 50ms.")
story += a(
    "I passi 2–9 hanno durate crescenti da 50ms a 100ms (incremento ~7ms/passo). "
    "I passi 10–19 scendono da 95ms a 50ms (~5ms/passo). "
    "Surplus redistribuito: 1/3 (89ms) sul primo passo 6, 2/3 (177ms) sul secondo "
    "passo 6. Durate finali: primo 6 = 168ms, secondo 6 = 242ms, passo 9 = 90ms. "
    "Durata totale esattamente 4000ms."
)

story += u("Lo shift orizzontale deve avvenire al passo 9 dell'animazione separatore.")
story += a(
    "Aggiunto flag pendingOffset: al cambio di minuto le cifre si animano subito, "
    "ma lo shift orizzontale viene ritardato fino a quando l'animazione del "
    "separatore raggiunge il passo 9 (◇ alto / vuoto basso). "
    "Il flag viene impostato nel loop() al cambio minuto e consumato nel timer "
    "del separatore quando colonPhase arriva a 8. I comandi seriali (p/l) "
    "mantengono lo shift immediato per praticità di test."
)
story += note("Compilato: 7458 byte flash (24%), 719 byte RAM (35%). Caricato.")

story += u("Sincronizzazione minuto basata sui cicli del separatore.")
story += a(
    "Eliminato il timer a secondi (TICK_MS, lastTick, ss). Il minuto è ora scandito "
    "dal contatore colonCycle: dopo 14 cicli completi del separatore (~56s) si avanza "
    "di un minuto. Sequenza: 14 cicli animazione separatore → cambio cifre → shift layout. "
    "Aggiunta funzione drawCycleHex() per debug: mostra il contatore cicli in esadecimale "
    "(0–D) nell'angolo più distante dall'orologio (sinistra o destra in base all'offset). "
    "La chiamata è commentata nel codice finale."
)
story += note("Compilato: 7372 byte flash (23%), 714 byte RAM (34%). Caricato.")

story += u("Controllo luminosità via software e nuovi comandi seriali.")
story += a(
    "Dal datasheet Futaba M202SD16FA (sez. 4-4-6 Function Set), i bit BR1/BR0 "
    "dell'istruzione Function Set controllano la luminosità del VFD: "
    "00=100%, 01=75%, 10=50%, 11=25%. "
    "Aggiunta funzione setBrightness(level): livello 0 spegne il display "
    "(comando Display OFF 0x08), livelli 1–4 impostano 25%/50%/75%/100%. "
    "All'avvio il display parte con luminosità minima (25%). "
    "Comandi seriali riprogrammati: "
    "m/-1min, M/+1min, h/-1ora, H/+1ora, d/-10min, D/+10min, "
    "b0/display off, b1/25%, b2/50%, b3/75%, b4/100% (prefisso 'b' per evitare "
    "conflitto con l'input HHMM)."
)

story += u("L'orologio guadagna 4 minuti per ora.")
story += a(
    "Causa: 14 cicli × 3981ms = 55734ms per minuto (non 60s). "
    "Fix: portato CYCLES_PER_MIN a 15 e aggiustato la durata del passo 1 "
    "da 2400 a 2419ms per un totale ciclo di esattamente 4000ms. "
    "15 × 4000 = 60000ms = 60s per minuto."
)
story += note("Compilato: 7644 byte flash (24%), 757 byte RAM (36%). Caricato.")

# ── INTEGRAZIONE RTC DS3231 ──────────────────────────────────────────────────
story += hr()
story += section("Integrazione RTC DS3231 (modulo ZS-042)")
story += u(
    "Vorrei che l'orario non sparisse quando scolgo l'orologio. "
    "Ho preso un modulo RTC di cui troverai il datasheet nella cartella."
)
story += a(
    "Il modulo è uno ZS-042 con chip DS3231SN, comunicazione I2C, batteria CR2032 "
    "per il backup. Cablaggio: VCC→5V, GND→GND, SDA→A4, SCL→A5 (I2C hardware "
    "dell'ATmega328P). I pin 32K, SQW e SPX non servono. I pull-up I2C sono "
    "integrati sulla scheda. Aggiunta libreria RTClib di Adafruit. "
    "All'avvio il codice legge l'ora dal DS3231 e parte subito; se il modulo non "
    "risponde mostra il prompt manuale. Quando si regola l'ora (comandi M/m/H/h/D/d "
    "o via seriale), viene aggiornato anche il DS3231."
)
story += note("Compilato: 11302 byte flash (36%), 1014 byte RAM (49%). Caricato.")

# ── ORA LEGALE AUTOMATICA ────────────────────────────────────────────────────
story += section("Ora legale automatica (DST EU)")
story += u("Se volessi tenere conto dell'ora legale? Il modulo RTC la supporta?")
story += a(
    "Il DS3231 non gestisce il DST — è un orologio UTC puro. Implementata la "
    "gestione software: il DS3231 memorizza sempre l'ora UTC; all'avvio e ad ogni "
    "cambio minuto il codice calcola l'ora locale italiana (CET=UTC+1, CEST=UTC+2). "
    "Funzioni aggiunte: lastSunday(year, month) calcola l'ultima domenica del mese; "
    "isDST(DateTime utc) ritorna true se siamo in ora legale EU (dall'ultima "
    "domenica di marzo ore 02:00 UTC all'ultima domenica di ottobre ore 03:00 UTC); "
    "utcToLocal() applica l'offset; localToUtc() lo inverte per salvare nel DS3231."
)

story += u("Come imposto data e ora?")
story += a(
    "Nuovo comando seriale s:DDMMYYYY-HHmmSS (es. s:22032026-143000 per il 22 marzo "
    "2026 alle 14:30:00 ora locale). Il codice parsa i valori, calcola se la data è "
    "in periodo DST, sottrae l'offset per ottenere UTC e salva nel DS3231 con "
    "rtc.adjust(). Il vecchio comando HHMM a 4 cifre è stato rimosso. "
    "Sulla seriale vengono stampati UTC, ora locale e stato DST."
)
story += note("Compilato: 13410 byte flash (43%), 1294 byte RAM (63%). Caricato.")

# ── ECHO SERIALE ─────────────────────────────────────────────────────────────
story += section("Echo seriale e help comandi")
story += u("Vorrei che la seriale gestisse l'eco dei caratteri.")
story += a(
    "Aggiunto echo: ogni carattere ricevuto (tranne newline) viene rimandato "
    "indietro con Serial.write(c). Al newline viene stampato Serial.println()."
)

story += u("Aggiorna l'intestazione seriale all'avvio.")
story += a(
    "Intestazione riscritta: riga 'Titolo: clockMoveSepAnimazione', "
    "riga 'Creato in char1 il 07 Mar 2026', riga 'Compilato: __DATE__ __TIME__'. "
    "Aggiunto blocco help con i comandi disponibili: "
    "s:DDMMYYYY-HHmmSS, M/m, D/d, H/h, b0-b4. "
    "Rimosso il comando HHMM a 4 cifre."
)
story += note("Compilato: 13522 byte flash (44%), 1196 byte RAM (58%). Caricato.")

# ── INDICATORE BATTERIA SCARICA ──────────────────────────────────────────────
story += section("Indicatore batteria RTC scarica")
story += u("Come è possibile accorgersi se la batteria è a fine vita?")
story += a(
    "Il DS3231 ha il bit OSF (Oscillator Stop Flag) che si alza quando l'oscillatore "
    "si ferma (batteria scarica). RTClib lo espone con rtc.lostPower(). "
    "Non c'è modo di leggere il voltaggio della CR2032 dal chip — servirebbe un "
    "circuito esterno. Implementato: all'avvio, se lostPower() è true, la variabile "
    "battLow diventa true. Nel loop il carattere 0x1D lampeggia nell'angolo in alto "
    "a sinistra (col 0, riga 0) con ritmo 0.3s visibile, 1.2s spento (ciclo ~1.5s). "
    "Se la posizione col 0 è occupata dall'orologio (offset negativo), il lampeggio "
    "si sposta in alto a destra (col 19). Dopo il comando s: il flag si resetta "
    "e il lampeggio si ferma."
)
story += note("Compilato: 13692 byte flash (44%), 1354 byte RAM (66%). Caricato.")

# ── LETTURA VOLTAGGIO BATTERIA ───────────────────────────────────────────────
story += section("Lettura voltaggio batteria CR2032 su pin analogico")
story += u(
    "Il modulo RTC non informa se la pila si sta scaricando, ma solo il fatto "
    "che l'oscillatore si è fermato. Posso collegare direttamente il polo positivo "
    "della batteria a un ingresso ADC?"
)
story += a(
    "Sì. La CR2032 ha tensione massima ~3.0V, dentro il range dell'ADC (0-5V), "
    "quindi non serve un partitore resistivo. Collegamento: polo + della CR2032 "
    "(sul modulo ZS-042) tramite resistenza da 100kΩ al pin A3 dell'Arduino Nano, "
    "GND già condiviso. La resistenza in serie è necessaria per evitare che la "
    "batteria alimenti il Nano attraverso i diodi di protezione interni del pin "
    "analogico (backfeed): senza resistenza, scollegando l'USB il LED power resta "
    "acceso. Con 100kΩ la corrente è limitata a ~30µA, insufficiente per il "
    "backfeed, e la lettura ADC resta accurata per il controllo soglia. "
    "Aggiunta funzione readBattMv() che legge analogRead(A3) e converte in millivolt. "
    "La funzione checkBatt() viene chiamata all'avvio e ad ogni cambio minuto: "
    "stampa il voltaggio sulla seriale e attiva il lampeggio batteria se scende "
    "sotto 2.5V (ma sopra 0.1V per ignorare letture con pin scollegato). "
    "Nell'help seriale aggiunta nota 'Batteria CR2032 letta su A3'."
)
story += note("Compilato: 14042 byte flash (45%), 1396 byte RAM (68%). Caricato.")

story += u("Il modulo ZS-042 ha un circuito di ricarica. È un problema?")
story += a(
    "Sì, è un difetto noto. Il ZS-042 ha un circuito che collega VCC (5V) alla "
    "batteria attraverso una resistenza (~200 ohm) e un diodo, pensato per "
    "batterie ricaricabili LIR2032. Con una CR2032 non ricaricabile il circuito "
    "non causa danni immediati ma è inutile e può far scaldare leggermente la "
    "batteria nel tempo. Il circuito non ha regolatore né cutoff di fine carica, "
    "quindi anche con una LIR2032 la tensione può salire oltre i 4.2V sicuri. "
    "Soluzione: rimuovere la resistenza R4 (o R5) sulla scheda per disabilitare "
    "il circuito di carica. Con CR2032 standard la pila dura 5-8 anni di backup."
)

# ── RIEPILOGO TECNICO ────────────────────────────────────────────────────────
story += hr()
story += section("Riepilogo tecnico")
story += [
    Paragraph(
        "<b>Hardware:</b> Arduino Nano + display LCD 20×2 HD44780 "
        "(Powertip PC1602ARS-CWA-A-Q). Interfaccia 4-bit. "
        "RTC DS3231 (modulo ZS-042) via I2C (SDA→A4, SCL→A5) con batteria CR2032.",
        ASSISTANT,
    ),
    Paragraph(
        "<b>Font system:</b> 8 tile CGRAM 5×8 px (A, flip(A), C, flip(C), F, G, "
        "flip(G), M) + caratteri ROM: 0x20 (SPACE) e 0x16 (verticale destra 3 px) "
        "come moduli gratuiti senza consumo di slot CGRAM. "
        "ROM 0x16 usato per: BR del digit 1, BR del digit 7. "
        "ROM 0x20 usato per: metà sinistra del digit 1, BL del digit 7, BL del digit 4.",
        ASSISTANT,
    ),
    Paragraph(
        "<b>Strumenti di sviluppo:</b> Python 3 + Pillow per la generazione dei "
        "PNG di anteprima; arduino-cli per compilazione e upload.",
        ASSISTANT,
    ),
    Paragraph(
        "<b>File prodotti:</b> generate.py, requirements.txt, output/digit_0–9.png, "
        "moduli/modulo_*.png, bigfont/bigfont.ino, clock/clock.ino, "
        "clockMov/clockMov.ino, clockSaveDispaly/clockSaveDispaly.ino, "
        "clockMoveSepAnimazione/clockMoveSepAnimazione.ino.",
        ASSISTANT,
    ),
    Paragraph(
        "<b>Digit 1 — posizionamento adattivo:</b> H1 +1 col quando H2=1; "
        "M1 -1 col quando M1=1; M2 -1 col per M2=1 e ulteriore -1 per M1=1. "
        "Cleanup finale dell'animazione condizionato a DIGITS[d][0]!=0x20 "
        "per evitare cancellazione della colonna vicina.",
        ASSISTANT,
    ),
    Paragraph(
        "<b>Shift orizzontale:</b> offset = (mm % 8) - 3, range -3..+4 (8 posizioni). "
        "Cambia a ogni minuto. Prima transizione cifra, poi spostamento layout.",
        ASSISTANT,
    ),
    Paragraph(
        "<b>RTC e DST:</b> il DS3231 memorizza UTC. All'avvio e ad ogni tick il codice "
        "converte in ora locale italiana (CET/CEST) usando le regole EU. "
        "Comando s:DDMMYYYY-HHmmSS per impostare data e ora locale. "
        "Indicatore batteria scarica: lampeggio 0x1C (0.3s ON, 1.2s OFF) in alto a sx/dx.",
        ASSISTANT,
    ),
    Spacer(1, 4),
    HRFlowable(width="100%", thickness=0.5, color=colors.HexColor("#cccccc")),
    Spacer(1, 4),
    Paragraph("Animazione di boot: DotDot", SECTION),
    Paragraph(
        "L'utente ha richiesto un'animazione astratta all'accensione, della durata di 2-4 secondi. "
        "Dopo aver esplorato diverse varianti in uno sketch di test separato (testBoot) con "
        "8 animazioni diverse — Matrix Rain, Glitch Pulse, Waveform, Nebula, Digital Rain, "
        "Breath Fade, Emergence, Cascade — è stata selezionata e raffinata l'animazione DotDot.",
        ASSISTANT,
    ),
    Paragraph(
        "<b>Concetto DotDot:</b> sfrutta la ridefinizione dinamica dei caratteri CGRAM. "
        "All'avvio gli 8 slot CGRAM vengono azzerati (tutti pixel spenti) e il display "
        "viene riempito con questi 8 caratteri distribuiti casualmente nelle 40 celle. "
        "Ad ogni iterazione vengono scelti 4 tile a caso e in ciascuno si accendono 8 pixel "
        "in posizioni casuali. Poiché lo stesso tile appare in più celle, ogni modifica CGRAM "
        "si riflette simultaneamente in tutte le celle che usano quel tile, creando un effetto "
        "di emersione progressiva e distribuita.",
        ASSISTANT,
    ),
    Paragraph(
        "<b>Fade out con split:</b> nella fase finale la luminosità cala gradualmente "
        "(da 4 a 1) mentre le due metà del display scorrono via dal centro — la metà sinistra "
        "verso sinistra, la destra verso destra. Contemporaneamente, 2 tile casuali perdono "
        "6 pixel ciascuno ad ogni passo, così i caratteri si dissolvono mentre escono.",
        ASSISTANT,
    ),
    Paragraph(
        "<b>Integrazione nel clock:</b> la funzione bootAnimation() viene chiamata nel setup() "
        "dopo l'inizializzazione LCD e prima del caricamento dei tile CGRAM dell'orologio. "
        "Usa variabili locali (104 byte sullo stack, liberati dopo). Il LED respiro resta attivo "
        "durante l'animazione grazie a bootWaitMs() che chiama updateBreathing(). "
        "I tile CGRAM dell'orologio (tile0-tile7) vengono caricati solo dopo l'animazione, "
        "evitando un doppio caricamento.",
        ASSISTANT,
    ),
]

# ── BOOT ANIMATION REVERSE + TRIPLE ───────────────────────────────────────
story += hr()
story += section("Boot animation reverse e sequenza tripla")
story += u(
    "Vorrei un'animazione inversa della DotDot: le cose arrivano dal bordo verso "
    "il centro e poi i pixel si spengono progressivamente."
)
story += a(
    "Creata bootAnimationReverse(): inverte entrambe le fasi. "
    "Fase 1 (merge): le colonne partono dai bordi e convergono verso il centro, "
    "con tile che acquistano pixel progressivamente e luminosità crescente. "
    "Fase 2 (decay): i pixel si spengono gradualmente con luminosità calante. "
    "La riga superiore parte prima della inferiore (stagger invertito rispetto alla DotDot). "
    "Nella split originale aggiunto stagger di 2 step: la riga inferiore inizia a separarsi "
    "prima di quella superiore, creando un effetto asimmetrico più naturale."
)
story += u(
    "Metti un bottone che quando premuto fa la sequenza completa: "
    "animazione normale + reverse + normale."
)
story += a(
    "Funzione tripleBootSequence() che esegue le tre animazioni in sequenza. "
    "Il bottone è collegato a D7 (INPUT_PULLUP) con D6 come massa software. "
    "Debounce semplice da 50ms nel loop(). Durante la pressione il LED respiro "
    "resta attivo. Dopo la sequenza i tile CGRAM vengono ricaricati e il display "
    "ridisegnato. La seriale stampa data e ora corrente prima della sequenza."
)

# ── BUZZER PIEZO ──────────────────────────────────────────────────────────
story += hr()
story += section("Buzzer piezo")
story += u(
    "Vorrei un click sonoro discreto al cambio del minuto."
)
story += a(
    "Buzzer piezo collegato a D9 (segnale, via resistenza 220Ω) e D8 (massa software). "
    "La funzione clickMinute() genera un impulso breve usando il Timer1 hardware "
    "(tone/noTone) per un suono pulito. Il click viene chiamato ad ogni avanzamento "
    "di minuto, sia automatico che manuale (comandi M/m/H/h/D/d)."
)

# ── AUTO-DIMMING ──────────────────────────────────────────────────────────
story += hr()
story += section("Auto-dimming basato su alba e tramonto")
story += u(
    "Vorrei che la luminosità si regoli automaticamente in base all'ora del giorno."
)
story += a(
    "Implementato un calcolo semplificato di alba e tramonto basato sulla declinazione "
    "solare e l'angolo orario (precisione ~10-15 minuti, sufficiente per il dimming). "
    "Le coordinate GPS sono salvate in EEPROM con magic byte di validazione. "
    "Default: Roma (41.9028, 12.4964). Tre fasce di luminosità: notte (25%), "
    "transizione alba/tramonto ±30min (50%), giorno pieno (100%). "
    "La funzione calcAutoBrightness() determina il livello, applyAutoBrightness() "
    "lo applica solo se cambiato. Il controllo manuale (comandi 0-4) sovrascrive "
    "l'automatico con flag brManualOverride; il comando '9' riattiva l'auto-dimming."
)

# ── GPS EEPROM ────────────────────────────────────────────────────────────
story += section("Coordinate GPS via seriale")
story += u(
    "Come imposto le coordinate?"
)
story += a(
    "Nuovo comando seriale p:lat,lon (es. p:41.9028,12.4964 — formato Google Maps). "
    "Il parser accetta virgola con spazi opzionali dopo. Le coordinate vengono validate "
    "(-90..90 lat, -180..180 lon), salvate in EEPROM e i tempi di alba/tramonto "
    "ricalcolati immediatamente. L'auto-dimming viene riapplicato subito."
)

# ── BREATHING LED ─────────────────────────────────────────────────────────
story += hr()
story += section("Breathing LED")
story += u(
    "Aggiungi un LED che respira collegato a D13."
)
story += a(
    "LED collegato a D13 con resistore. La funzione updateBreathing() genera un PWM "
    "software con curva sinusoidale. Il LED è sempre attivo, anche durante le animazioni "
    "di boot grazie a bootWaitMs() che chiama updateBreathing() nel ciclo di attesa. "
    "Il pin viene inizializzato nel setup() come OUTPUT."
)

# ── SOFTWARE RESET ────────────────────────────────────────────────────────
story += hr()
story += section("Comandi aggiuntivi")
story += a(
    "Aggiunti ulteriori comandi seriali: 'i' per ripetere l'animazione di boot "
    "(i tile CGRAM vengono ricaricati dopo), 'v' per leggere il voltaggio della "
    "batteria CR2032, 'r' per un reset software (spegne il display, attende 3 secondi, "
    "poi salta all'indirizzo 0 con asm volatile). "
    "Rimosso il prefisso 'b' dai comandi di luminosità: ora si usano direttamente "
    "i tasti 0-4 e 9 per auto."
)

# ── MODIFICA MODULO ZS-042 ────────────────────────────────────────────────
story += hr()
story += section("Modifica hardware modulo ZS-042")
story += a(
    "Il modulo ZS-042 include un circuito di ricarica (resistenza + diodo) pensato "
    "per batterie ricaricabili LIR2032. Usando una CR2032 non ricaricabile, è stata "
    "rimossa la resistenza in serie al diodo che arriva alla pila per disabilitare "
    "il circuito di ricarica ed evitare danni alla cella. "
    "Inoltre, un filo è stato saldato al terminale positivo del portabatteria e "
    "collegato al pin A3 dell'Arduino (tramite resistenza da 100kΩ) per consentire "
    "il monitoraggio del voltaggio della CR2032."
)

# ── TRADUZIONE IN INGLESE ─────────────────────────────────────────────────
story += hr()
story += section("Traduzione in inglese")
story += a(
    "Tutto il codice sorgente, i commenti, le stringhe seriali e la documentazione "
    "sono stati tradotti dall'italiano all'inglese per la pubblicazione su GitHub. "
    "Circa 60 modifiche nel file .ino. I nomi delle funzioni e delle variabili "
    "erano già in inglese."
)

# ── SYNC SECONDI AL BOOT ─────────────────────────────────────────────────
story += hr()
story += section("Sincronizzazione secondi al boot")
story += u(
    "Alla partenza, quando viene letto il tempo dal modulo RTC, come vengono "
    "troncati i secondi? È possibile fare meglio?"
)
story += a(
    "Il problema: utcToLocal() estraeva solo ore e minuti dall'RTC, ignorando i "
    "secondi. Il timer interno partiva sempre da zero, causando uno sfasamento "
    "fisso fino a ~59 secondi che non si correggeva nemmeno con il re-sync periodico "
    "(anch'esso ignorava i secondi). "
    "Soluzione: startClock() accetta ora un terzo parametro opzionale 'sec'. Al boot "
    "vengono letti i secondi dall'RTC e passati a startClock(), che calcola quanti "
    "cicli di animazione (4s ciascuno) sono già trascorsi (colonCycle = sec/4) e "
    "retrodata lastColon del resto (sec%4 * 1000ms). Così il primo scatto di minuto "
    "avviene al secondo :00 reale e tutti i re-sync successivi restano allineati. "
    "Lo stesso meccanismo è applicato anche al comando seriale s: che imposta "
    "data e ora manualmente."
)

# ---------------------------------------------------------------------------
# Generazione PDF
# ---------------------------------------------------------------------------
out = "/Users/ghedo/script/AllClaude/char1/conversazione.pdf"

doc = SimpleDocTemplate(
    out,
    pagesize=A4,
    leftMargin=2.5*cm,
    rightMargin=2.5*cm,
    topMargin=2.5*cm,
    bottomMargin=2.5*cm,
    title="Progetto Big-Font LCD — Trascrizione conversazione",
    author="Claude + Utente",
)
doc.build(story)
print(f"PDF generato: {out}")
