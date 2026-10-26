#include "i18n.h"
#include "pet.h"        // MED_COUNT
#include <Preferences.h>

Lang gLang = LANG_DEFAULT;

// String table [language][id]. No accents or n-tildes: the firmware's bitmap
// font does not have them (that is why the Spanish already read "Esta", "bano", etc.).
static const char *const STRINGS[LANG_COUNT][STR_COUNT] = {
  // ---------------- ES ----------------
  {
    "Est\240 evolucionando!", "Nam nam!", "Le gusta!", "Tiene hambre!", "Necesita un ba\244o!",
    "Est\240 agotado...", "Est\240 triste...", "Est\240 rellenito...", "Es SHINY!!", "Est\240 feliz",
    "GRACIAS! Hasta siempre", "Se ha escapado...", "Adi\242s! Se despide...",
    "HUEVO", "Huevo legendario!?", "Huevo raro!", "Toca el huevo...", "Se mueve!", "Est\240 a punto!",
    "POKEDEX %u/151",
    "POKEDEX", "Todos", "Criados", "Atrapados", "Shiny",
    "Soltar a %s?", "SI", "NO",
    "%u GOLPES", "NUEVO RECORD!", "RECORD: %u", "APORREA RAPIDO!",
    "PUNTOS: %u", "Qu\202 felicidad!", "+felicidad",
    "AJUSTAR HORA", "HORA", "MIN",
    "MEDALLA!", "GENIAL!", "RACHA %u DIAS!",
    "VIN", "???", "Manzana", "Hamburguesa", "Pizza",
    "Nivel", "Racha (r\202cord)", "V\241nculo", "Comida favorita", "Edad",
    "%lud",
    "COMBATE", "ESTADISTICAS", "PS", "FUE", "DEF", "VEL", "PES", "VICTORIAS", "DERROTAS",
    "MEDALLAS",
    "NOMBRE:",
    "COM", "FEL", "ENE", "LIM",
    "REC %u",
    "PROGRESO", "Nv.%u", "%u min para Nv.%u", "EVOLUCION", "Forma final",
    "Listo para evolucionar!", "Sube todo a 40 para evolucionar",
    "Evoluciona en %u niv.", "Descuidos: %u",
    "EVOLUCIONAR", "%s quiere decirte algo...", "%s se siente abandonado...",
    "Evolucionar?", "Mantener forma", "Despedirse?", "Despedirse", "Quedaros juntos",
    "Elige tu inicial",
    "Sin sprites", "C\240rgalos en la SD",
    "AJUSTES", "Apagar", "Audio / V\241deo", "Hora", "Idioma",
    "Sonido", "Brillo", "On", "Off",
    //#region ES wild battle strings
    "COMBATE SALVAJE", "Combate salvaje?", "LUCHAR", "Luego", "HUIR",
    "Atacar", "Evadir", "Curar", "R\240pido", "Fuerte",
    "VICTORIA!", "DERROTA", "%u rondas", "Hecho %u / Recibido %u", "R%u",
    "Atac\242 por %u", "Efectivo", "D\202bil", "Fall\242!", "Esquivado", "Rival Esquiv\242",
    "Contra Listo", "Sin Descanso", "Cura +%u PS", "Guardia",
    "Atrapar", "Dejar", "Atrapado!", "Se Fue", "Cerca!",
    "FUE +%u", "DEF +%u", "VEL +%u", "OK", "ATRAPADO",
    //#endregion
    //#region ES WiFi strings
    "WiFi", "Buscar", "Buscando...", "No hay redes",
    "Conectando...", "Conectado", "Error al conectar", "Abierta", "CONTRASE\245A:",
    //#endregion
  },
  // ---------------- EN ----------------
  {
    "Evolving!", "Yum yum!", "It likes it!", "It's hungry!", "Needs a bath!",
    "Worn out...", "Feeling sad...", "A bit chubby...", "It's SHINY!!", "It's happy",
    "THANKS! Farewell", "It ran away...", "Bye! Waving goodbye...",
    "EGG", "Legendary egg!?", "Rare egg!", "Tap the egg...", "It moves!", "Almost there!",
    "POKEDEX %u/151",
    "POKEDEX", "All", "Raised", "Caught", "Shiny",
    "Release %s?", "YES", "NO",
    "%u HITS","NEW RECORD!", "BEST: %u", "HIT FAST!",
    "SCORE: %u", "So much fun!", "+happiness",
    "SET TIME", "HOUR", "MIN",
    "MEDAL!", "AWESOME!", "%u DAY STREAK!",
    "BOND", "???", "Apple", "Hamburger", "Pizza",
    "Level", "Streak (Best)", "Bond", "Fav Food", "Age",
    "%lud",
    "BATTLE", "STATS", "HP", "ATK", "DEF", "SPD", "WGT", "WINS", "LOSSES",
    "MEDALS",
    "NAME:",
    "FOOD", "JOY", "ENE", "HYG",
    "BEST %u",
    "PROGRESS", "Lv.%u", "%u min to Lv.%u", "EVOLUTION", "Final form",
    "Ready to evolve!", "All needs >=40 to evolve",
    "Evolves in %u lv.", "Slip-ups: %u",
    "EVOLVE!", "%s wants to tell you...", "%s feels abandoned...",
    "Evolve?", "Keep form", "Say goodbye?", "Goodbye", "Stay together",
    "Choose your starter",
    "No sprites", "Load them onto the SD",
    "SETTINGS", "Screen Off", "Audio / Video", "Time", "Language",
    "Sound", "Brightness", "On", "Off",
    //#region EN wild battle strings
    "WILD BATTLE", "Wild battle?", "FIGHT", "Later", "RUN",
    "Attack", "Dodge", "Rest", "Quick", "Heavy",
    "WIN!", "LOSS", "%u rounds", "Dealt %u / Taken %u", "R%u",
    "Attacked for %u", "Effective", "Weak", "Missed!", "Dodged", "Enemy Dodged",
    "Counter Ready", "No Rest Left", "Rest +%u HP", "Guard",
    "Catch", "Leave", "Caught!", "Escaped", "Close!",
    "ATK +%u", "DEF +%u", "SPD +%u", "OK", "CAUGHT",
    //#endregion
    //#region EN WiFi strings
    "WiFi", "Scan", "Scanning...", "No networks found",
    "Connecting...", "Connected", "Connection failed", "Open", "PASSWORD:",
    //#endregion
  },
  // ---------------- FR ----------------
  {
    "Il \202volue!", "Miam miam!", "Il aime \207a!", "Il a faim!", "Besoin d'un bain!",
    "\220puis\202...", "Triste...", "Un peu rond...", "C'est SHINY!!", "Il est content",
    "MERCI! Adieu", "Il s'est enfui...", "Au revoir!",
    "OEUF", "Oeuf l\202gendaire!?", "Oeuf rare!", "Touche l'oeuf...", "Il bouge!", "Presque l\205!",
    "POKEDEX %u/151",
    "POKEDEX", "Tous", "\220lev\202s", "Attrap\202s", "Chroma",
    "Rel\203cher %s?", "OUI", "NON",
    "%u COUPS","NOUVEAU RECORD!", "RECORD: %u", "FRAPPE VITE!",
    "SCORE: %u", "Trop bien!", "+bonheur",
    "R\220GLER L'HEURE", "HEURE", "MIN",
    "M\220DAILLE!", "SUPER!", "S\220RIE %u JOURS!",
    "LIEN", "???", "Pomme", "Hamburger", "Pizza",
    "Niveau", "S\202rie (record)", "Lien", "Plat pr\202f\202r\202", "Age",
    "%luj",
    "COMBAT", "STATS", "PV", "ATQ", "DEF", "VIT", "PDS", "VICTOIRES", "D\220FAITES",
    "M\220DAILLES",
    "NOM:",
    "NOUR", "JOIE", "ENE", "HYG",
    "REC %u",
    "PROGRES", "Niv.%u", "%u min pour Niv.%u", "\220VOLUTION", "Forme finale",
    "Pr\210t \205 \202voluer!", "Tout \205 40 pour \202voluer",
    "\220volue dans %u niv.", "N\202gligences: %u",
    "\220VOLUER", "%s veut te parler...", "%s se sent abandonn\202...",
    "\220voluer?", "Garder forme", "Dire adieu?", "Adieu", "Rester ensemble",
    "Choisis ton starter",
    "Pas de sprites", "Charge-les sur la SD",
    "R\220GLAGES", "\220teindre", "Audio / Vid\202o", "Heure", "Langue",
    "Son", "Luminosit\202", "On", "Off",
    //#region FR wild battle strings
    "COMBAT SAUVAGE", "Combat sauvage?", "COMBAT", "Apr\212s", "FUIR",
    "Attaque", "Esquive", "Repos", "Rapide", "Fort",
    "VICTOIRE!", "D\220FAITE", "%u tours", "Inflig\202s %u / Re\207us %u", "M%u",
    "Attaque %u", "Efficace", "Faible", "Rat\202!", "Esquiv\202", "Ennemi Esquive",
    "Contre Pr\210t", "Plus De Repos", "Soin +%u PV", "Garde",
    "Capture", "Laisser", "Captur\202!", "\220chapp\202", "Presque!",
    "ATQ +%u", "DEF +%u", "VIT +%u", "OK", "CAPTUR\220",
    //#endregion
    //#region FR WiFi strings
    "WiFi", "Chercher", "Recherche...", "Aucun r\202seau",
    "Connexion...", "Connect\202", "\220chec connexion", "Ouvert", "MOT DE PASSE:",
    //#endregion
  },
  // ---------------- DE ----------------
  // German does use umlauts: the firmware's 5x7 font is a full CP437 table
  // (256 glyphs), so the single byte for each character is enough.
  // They are written in octal so they do not depend on the source file encoding:
  //   \204 a   \224 o   \201 u   \216 A   \231 O   \232 U   \341 ss
  // One byte per character: strlen() still measures correctly and centering does not change.
  {
    "Entwickelt sich!", "Mampf mampf!", "Gef\204llt ihm!", "Hat Hunger!", "Braucht ein Bad!",
    "Ersch\224pft...", "Traurig...", "Etwas rundlich...", "Es ist SHINY!!", "Es ist froh",
    "DANKE! Leb wohl", "Es ist weg...", "Tsch\201ss! Winkt",
    "EI", "Legend\204res Ei!?", "Seltenes Ei!", "Ber\201hre das Ei...", "Es bewegt sich!", "Fast so weit!",
    "POKEDEX %u/151",
    "POKEDEX", "Alle", "Gez\201chtet", "Gefangen", "Schillernd",
    "%s freilassen?", "JA", "NEIN",
    "%u TREFFER","NEUER REKORD!", "REKORD: %u", "SCHNELL HAUEN!",
    "PUNKTE: %u", "Wie sch\224n!", "+Freude",
    "ZEIT STELLEN", "STD", "MIN",
    "MEDAILLE!", "TOLL!", "%u TAGE SERIE!",
    "BND", "???", "Apfel", "Hamburger", "Pizza",
    "Level", "Serie (Rekord)", "Bindung", "Lieblingsessen", "Alter",
    "%luT",
    "KAMPF", "WERTE", "KP", "ANG", "VER", "INI", "GEW", "SIEGE", "NIEDERLAGEN",
    "MEDAILLEN",
    "NAME:",
    "ESS", "FRO", "ENE", "HYG",
    "REK %u",
    "FORTSCHRITT", "Lv.%u", "%u min bis Lv.%u", "ENTWICKLUNG", "Endform",
    "Bereit zur Entwicklung!", "Alles >=40 zur Entwicklung",
    "Entwicklung in %u Lv.", "Patzer: %u",
    "ENTWICKELN", "%s will dir etwas sagen...", "%s f\201hlt sich verlassen...",
    "Entwickeln?", "Form behalten", "Abschied?", "Leb wohl", "Zusammen bleiben",
    "W\204hle deinen Starter",
    "Keine Sprites", "Auf die SD laden",
    "EINSTELLUNGEN", "Bildschirm aus", "Audio / Video", "Uhrzeit", "Sprache",
    "Ton", "Helligkeit", "An", "Aus",
    //#region DE wild battle strings
    "WILDER KAMPF", "Wilder Kampf?", "KAMPF", "Sp\204ter", "FLIEHEN",
    "Angriff", "Ducken", "Ruhen", "Schnell", "Stark",
    "SIEG!", "NIEDERLAGE", "%u Runden", "Verteilt %u / Erlitten %u", "R%u",
    "Angriff: %u", "Effektiv", "Schwach", "Verfehlt!", "Ausgewichen", "Gegner Wich Aus",
    "Konter Bereit", "Keine Ruhe", "Heilung +%u KP", "Schutz",
    "Fangen", "Lassen", "Gefangen!", "Entkommen", "Knapp!",
    "ANG +%u", "VER +%u", "INI +%u", "OK", "GEFANGEN",
    //#endregion
    //#region DE WiFi strings
    "WLAN", "Suchen", "Suche...", "Keine Netzwerke",
    "Verbinde...", "Verbunden", "Fehlgeschlagen", "Offen", "PASSWORT:",
    //#endregion
  },
  // ---------------- IT ----------------
  {
    "Si evolve!", "Gnam gnam!", "Gli piace!", "Ha fame!", "Vuole un bagno!",
    "Esausto...", "Triste...", "Un po' cicciotto...", "E' SHINY!!", "E' felice",
    "GRAZIE! Addio", "E' scappato...", "Ciao! Saluta",
    "UOVO", "Uovo leggendario!?", "Uovo raro!", "Tocca l'uovo...", "Si muove!", "Ci siamo quasi!",
    "POKEDEX %u/151",
    "POKEDEX", "Tutti", "Allevati", "Catturati", "Cromatici",
    "Liberare %s?", "SI", "NO",
    "%u COLPI","NUOVO RECORD!", "RECORD: %u", "COLPISCI VELOCE!",
    "PUNTI: %u", "Che gioia!", "+felicit\205",
    "IMPOSTA ORA", "ORA", "MIN",
    "MEDAGLIA!", "GRANDE!", "SERIE %u GIORNI!",
    "LEG", "???", "Mela", "Hamburger", "Pizza",
    "Livello", "Serie (record)", "Legame", "Cibo preferito", "Et\205",
    "%lug",
    "LOTTA", "STATISTICHE", "PS", "ATT", "DIF", "VEL", "PES", "VITTORIE", "SCONFITTE",
    "MEDAGLIE",
    "NOME:",
    "CIB", "GIO", "ENE", "IGI",
    "REC %u",
    "PROGRESSI", "Lv.%u", "%u min per Lv.%u", "EVOLUZIONE", "Forma finale",
    "Pronto a evolvere!", "Tutto a 40 per evolvere",
    "Evolve tra %u liv.", "Disattenzioni: %u",
    "EVOLVI", "%s vuole dirti qualcosa...", "%s si sente abbandonato...",
    "Evolvere?", "Mantieni forma", "Salutare?", "Addio", "Restare insieme",
    "Scegli l'iniziale",
    "Senza sprite", "Caricali sulla SD",
    "IMPOSTAZIONI", "Spegni schermo", "Audio / Video", "Ora", "Lingua",
    "Suono", "Luminosit\205", "On", "Off",
    //#region IT wild battle strings
    "LOTTA SELVATICA", "Lotta selvatica?", "LOTTA", "Dopo", "FUGGI",
    "Attacca", "Schiva", "Riposa", "Rapido", "Forte",
    "VITTORIA!", "SCONFITTA", "%u turni", "Inflitti %u / Subiti %u", "T%u",
    "Attacco: %u", "Efficace", "Debole", "Mancato!", "Schivato", "Nemico Schiva",
    "Contrattacco", "Niente Riposo", "Cura +%u PS", "Guardia",
    "Prendi", "Lascia", "Preso!", "Fuggito", "Quasi!",
    "ATT +%u", "DIF +%u", "VEL +%u", "OK", "PRESO",
    //#endregion
    //#region IT WiFi strings
    "WiFi", "Cerca", "Ricerca...", "Nessuna rete",
    "Connessione...", "Connesso", "Connessione fallita", "Aperta", "PASSWORD:",
    //#endregion
  },
  // ---------------- PT ----------------
  {
    "Evoluindo!", "Nham nham!", "Ele gosta!", "Est\240 com fome!", "Precisa de banho!",
    "Exausto...", "Triste...", "Um pouco gordinho...", "\220 SHINY!!", "Est\240 feliz",
    "OBRIGADO! Adeus", "Fugiu...", "Tchau! Acena",
    "OVO", "Ovo lend\240rio!?", "Ovo raro!", "Toque no ovo...", "Mexe-se!", "Quase l\240!",
    "POKEDEX %u/151",
    "POKEDEX", "Todos", "Criados", "Apanhados", "Brilhantes",
    "Soltar %s?", "SIM", "NAO",
    "%u GOLPES","NOVO RECORDE!", "RECORDE: %u", "BATA RAPIDO!",
    "PONTOS: %u", "Que alegria!", "+alegria",
    "AJUSTAR HORA", "HORA", "MIN",
    "MEDALHA!", "OTIMO!", "%u DIAS SEGUIDOS!",
    "LA\200O", "???", "Ma\207a", "Hamburguer", "Pizza",
    "N\241vel", "Sequ\210ncia (rec.)", "La\207o", "Comida favorita", "Idade",
    "%lud",
    "COMBATE", "ESTATISTICAS", "PS", "ATQ", "DEF", "VEL", "PES", "VITORIAS", "DERROTAS",
    "MEDALHAS",
    "NOME:",
    "COM", "ALE", "ENE", "HIG",
    "REC %u",
    "PROGRESSO", "Niv.%u", "%u min para Niv.%u", "EVOLUCAO", "Forma final",
    "Pronto a evoluir!", "Tudo a 40 para evoluir",
    "Evolui em %u niv.", "Descuidos: %u",
    "EVOLUIR", "%s quer dizer-te algo...", "%s sente-se abandonado...",
    "Evoluir?", "Manter forma", "Despedir?", "Adeus", "Ficar juntos",
    "Escolhe o inicial",
    "Sem sprites", "Carrega-os no SD",
    "AJUSTES", "Desligar visor", "Audio / V\241deo", "Hora", "Idioma",
    "Som", "Brilho", "On", "Off",
    //#region PT wild battle strings
    "BATALHA SELVAGEM", "Batalha selvagem?", "LUTAR", "Depois", "FUGIR",
    "Atacar", "Desviar", "Curar", "R\240pido", "Forte",
    "VITORIA!", "DERROTA", "%u rondas", "Causado %u / Sofrido %u", "R%u",
    "Atacou por %u", "Efetivo", "Fraco", "Falhou!", "Desviou", "Rival Desviou",
    "Contra Pronto", "Sem Descanso", "Cura +%u PS", "Guarda",
    "Apanhar", "Deixar", "Apanhado!", "Fugiu", "Perto!",
    "ATQ +%u", "DEF +%u", "VEL +%u", "OK", "APANHADO",
    //#endregion
    //#region PT WiFi strings
    "WiFi", "Procurar", "A procurar...", "Nenhuma rede",
    "A ligar...", "Ligado", "Falha ao ligar", "Aberta", "PALAVRA-PASSE:",
    //#endregion
  },
  // ---------------- JA ----------------
  {
    "しんかした！", "おいしい！", "うれしそう！", "おなかすいた！", "おふろにいれて！",
    "つかれた...", "かなしい...", "ちょっとふとった...", "いろちがい！！", "ごきげん！",
    "ありがとう！ またね", "にげちゃった...", "バイバイ！",

    "たまご", "でんせつのたまご！？", "めずらしいたまご！", "たまごをタッチ...", "うごいた！", "もうすぐ！",

    "ずかん %u/151",

    "ずかん", "ぜんぶ", "そだてた", "つかまえた", "いろちがい",

    "%s とおわかれ？", "はい", "いいえ",

    "わざ %u","しんきろく！", "きろく: %u", "れんだ！",

    "スコア: %u", "たのしい！", "+ごきげん",

    "じかんせってい", "じ", "ふん",

    "メダル！", "すごい！", "%u にちれんぞく！",

    "なかよし", "???", "りんご", "ハンバーガー", "ピザ",

    "レベル", "れんぞく(きろく)", "なかよし", "すきなごはん", "ねんれい",

    "%luにち",

    "バトル", "ステータス", "HP", "こうげき", "ぼうぎょ", "すばやさ", "たいじゅう", "かち", "まけ",

    "メダル",

    "なまえ:",

    "ごはん", "ごきげん", "げんき", "きれい",

    "きろく %u",

    "せいちょう", "Lv.%u", "あと%uふんで Lv.%u", "しんか", "さいしゅうけい",

    "しんかできる！", "ぜんぶ40でしんか",

    "%u Lv.でしんか", "おせわミス: %u",

    "しんか", "%sがなにかいいたそう...", "%sがさびしがっている...",

    "しんかする？", "このまま", "おわかれする？", "さようなら", "いっしょにいる",

    "さいしょのポケモンをえらぶ",

    "スプライトなし", "SDにいれてください",

    "せってい", "がめんオフ", "おと / がめん", "じかん", "ことば",

    "おと", "あかるさ", "オン", "オフ",
    //#region JA wild battle strings
    "やせいのバトル", "やせいのバトル？", "たたかう", "あとで", "にげる",
    "こうげき", "よける", "やすむ", "クイック", "パワー",
    "かった！", "まけた...", "%uターン", "あたえた %u / うけた %u", "T%u",
    "こうげき %u", "こうかばつぐん！", "いまひとつ...", "はずれた！", "よけた", "あいてがよけた！",
    "カウンターOK", "もうやすめない", "かいふく +%u HP", "ガード",
    "つかまえる", "にがす", "つかまえた！", "にげられた...", "おしい！",
    "こうげき +%u", "ぼうぎょ +%u", "すばやさ +%u", "OK", "ゲット",
    //#endregion
    //#region JA WiFi strings
    "Wi-Fi", "スキャン", "スキャンちゅう...", "みつかりません",
    "せつぞくちゅう...", "せつぞくずみ", "せつぞくしっぱい", "オープン", "パスワード:",
    //#endregion
  },
  // ---------------- KO ----------------
  // The specifiers go in the SAME ORDER as in the English row even though
  // natural Korean would place them differently (e.g. S_NEXT_LVL_FMT):
  // T(id) is filled with varargs in a fixed order at each call, so reordering
  // %u/%s here without touching the call site would silently swap which value
  // lands in which slot.
  {
    "진화 중!", "냠냠!", "마음에 들어해요!", "배고파요!", "목욕이 필요해요!",
    "지쳤어요...", "슬퍼해요...", "조금 통통해요...", "이로치예요!!", "행복해요",
    "고마워요! 안녕", "가출했어요...", "잘 가! 손을 흔들며...",
    "알", "전설의 알!?", "희귀한 알!", "알을 톡톡...", "움직여요!", "거의 다 됐어요!",
    "도감 %u/151",
    "도감", "전체", "키운", "잡은", "이로치",
    "%s 놓아줄까요?", "예", "아니오",
    "%u회 명중","신기록!", "최고: %u", "빠르게!",
    "점수: %u", "정말 즐거워요!", "+행복",
    "시간 설정", "시", "분",
    "메달!", "최고예요!", "%u일 연속!",
    "유대감", "???", "사과", "햄버거", "피자",
    "레벨", "연속 (최고)", "유대감", "좋아하는 음식", "나이",
    "%lu일",
    "배틀", "능력치", "HP", "공격", "방어", "속도", "무게", "승리", "패배",
    "메달",
    "이름:",
    "포만", "기분", "기력", "청결",
    "최고 %u",
    "진행도", "Lv.%u", "%u분 후 Lv.%u", "진화", "최종 형태",
    "진화 준비 완료!", "진화하려면 전부 40 이상",
    "%u레벨 이후에 진화", "실수: %u",
    "진화!", "%s가 할 말이 있대요...", "%s가 버려진 기분이래요...",
    "진화할까요?", "형태 유지", "작별할까요?", "작별", "계속 함께",
    "스타터를 선택하세요",
    "스프라이트 없음", "SD 카드에 넣어주세요",
    "설정", "화면 끄기", "소리 / 화면", "시간", "언어",
    "소리", "밝기", "켬", "끔",
    //#region KO wild battle strings
    "야생 배틀", "야생 배틀?", "싸운다", "나중에", "도망",
    "공격", "회피", "휴식", "빠르게", "강하게",
    "승리!", "패배...", "%u라운드", "내 공격 %u / 상대 공격 %u", "R%u",
    "공격 %u", "효과 굉장!", "효과 별로...", "빗나갔다!", "피했다", "상대가 피했다!",
    "반격 준비", "휴식 불가", "회복 +%u HP", "가드",
    "잡기", "놓아주기", "잡았다!", "도망쳤다...", "아깝다!",
    "공격 +%u", "방어 +%u", "속도 +%u", "OK", "포획",
    //#endregion
    //#region KO WiFi strings
    "Wi-Fi", "검색", "검색 중...", "네트워크 없음",
    "연결 중...", "연결됨", "연결 실패", "개방", "비밀번호:",
    //#endregion
  },
};

// Medal names in their three lengths [language][medal].
static const char *const MED_NAME[LANG_COUNT][MED_COUNT] = {
  { "Nv.10", "Nv.25", "Nv.50", "COMIDA", "RACHA 7", "VINCULO", "FORMA TOPE", "EN FORMA" },
  { "Lv.10", "Lv.25", "Lv.50", "FOOD", "7 STREAK", "BOND", "TOP FORM", "IN SHAPE" },
  { "Niv.10", "Niv.25", "Niv.50", "PLAT", "SERIE 7", "LIEN", "FORME MAX", "EN FORME" },
  { "Lv.10", "Lv.25", "Lv.50", "ESSEN", "7 SERIE", "BINDUNG", "ENDFORM", "FIT" },
  { "Lv.10", "Lv.25", "Lv.50", "CIBO", "SERIE 7", "LEGAME", "FORMA MAX", "IN FORMA" },
  { "Niv.10", "Niv.25", "Niv.50", "COMIDA", "SEQ 7", "LACO", "FORMA MAX", "EM FORMA" },
  { "Lv.10", "Lv.25", "Lv.50", "ごはん", "7にち", "なかよし", "さいしゅう", "げんき" },
  { "Lv.10", "Lv.25", "Lv.50", "음식", "연속 7일", "유대", "최고 컨디션", "건강함" },
};
static const char *const MED_LBL[LANG_COUNT][MED_COUNT] = {
  { "Nv10", "Nv25", "Nv50", "COMIDA", "7DIAS", "VINC", "TOPE", "SANO" },
  { "Lv10", "Lv25", "Lv50", "FOOD", "7DAYS", "BOND", "TOP", "FIT" },
  { "Niv10", "Niv25", "Niv50", "PLAT", "7JRS", "LIEN", "MAX", "FORME" },
  { "Lv10", "Lv25", "Lv50", "ESSEN", "7TAGE", "BND", "END", "FIT" },
  { "Lv10", "Lv25", "Lv50", "CIBO", "7GG", "LEG", "MAX", "FIT" },
  { "Niv10", "Niv25", "Niv50", "COMIDA", "7DIAS", "LACO", "MAX", "FIT" },
  { "Lv10", "Lv25", "Lv50", "ごはん", "7にち", "なかよし", "しんか", "げんき" },
  { "Lv10", "Lv25", "Lv50", "음식", "7일", "유대", "최고", "건강" },
};
static const char *const MED_DSC[LANG_COUNT][MED_COUNT] = {
  { "NIVEL 10", "NIVEL 25", "NIVEL 50", "COMIDA FAV",
    "RACHA 7 DIAS", "VINCULO MAX", "FORMA FINAL", "EN FORMA" },
  { "LEVEL 10", "LEVEL 25", "LEVEL 50", "FOOD FOUND",
    "7 DAY STREAK", "MAX BOND", "FINAL FORM", "IN SHAPE" },
  { "NIVEAU 10", "NIVEAU 25", "NIVEAU 50", "PLAT TROUVE",
    "SERIE 7 JOURS", "LIEN MAX", "FORME FINALE", "EN FORME" },
  { "LEVEL 10", "LEVEL 25", "LEVEL 50", "ESSEN GEFUNDEN",
    "7 TAGE SERIE", "MAX BINDUNG", "ENDFORM", "FIT" },
  { "LIVELLO 10", "LIVELLO 25", "LIVELLO 50", "CIBO TROVATO",
    "SERIE 7 GIORNI", "LEGAME MAX", "FORMA FINALE", "IN FORMA" },
  { "NIVEL 10", "NIVEL 25", "NIVEL 50", "COMIDA FAV",
    "SEQ 7 DIAS", "LACO MAX", "FORMA FINAL", "EM FORMA" },
  { "レベル 10", "レベル 25", "レベル 50", "ごはん はっけん",
    "7にち れんぞく", "なかよし MAX", "さいしゅうしんか", "げんきいっぱい" },
  { "레벨 10", "레벨 25", "레벨 50", "음식 발견",
    "연속 7일", "유대감 최대", "최종 진화", "건강함" },
};

#pragma region Battle type labels
// Short uppercase battle type labels [language][type], index = TYPE_* in dex.h
// (TYPE_NONE first, always ""). Latin labels stay at 8 characters or fewer and
// use CP437 single-byte capitals (\220 E-acute, \216 A-umlaut, \200 C-cedilla).
static const char *const TYPE_NAMES[LANG_COUNT][TYPE_NAME_COUNT] = {
  { "", "NORMAL", "FUEGO", "AGUA", "ELECTR", "PLANTA", "HIELO", "LUCHA", "VENENO", "TIERRA",
    "VOLADOR", "PSIQUICO", "BICHO", "ROCA", "FANTASMA", "DRAGON", "SINIEST", "ACERO", "HADA" },
  { "", "NORMAL", "FIRE", "WATER", "ELECTRIC", "GRASS", "ICE", "FIGHTING", "POISON", "GROUND",
    "FLYING", "PSYCHIC", "BUG", "ROCK", "GHOST", "DRAGON", "DARK", "STEEL", "FAIRY" },
  { "", "NORMAL", "FEU", "EAU", "\220LECTRIK", "PLANTE", "GLACE", "COMBAT", "POISON", "SOL",
    "VOL", "PSY", "INSECTE", "ROCHE", "SPECTRE", "DRAGON", "T\220N\220BRES", "ACIER", "F\220E" },
  { "", "NORMAL", "FEUER", "WASSER", "ELEKTRO", "PFLANZE", "EIS", "KAMPF", "GIFT", "BODEN",
    "FLUG", "PSYCHO", "K\216FER", "GESTEIN", "GEIST", "DRACHE", "UNLICHT", "STAHL", "FEE" },
  { "", "NORMALE", "FUOCO", "ACQUA", "ELETTRO", "ERBA", "GHIACCIO", "LOTTA", "VELENO", "TERRA",
    "VOLANTE", "PSICO", "COLEOT", "ROCCIA", "SPETTRO", "DRAGO", "BUIO", "ACCIAIO", "FOLLETTO" },
  { "", "NORMAL", "FOGO", "AGUA", "EL\220TRICO", "PLANTA", "GELO", "LUTADOR", "VENENOSO", "TERRA",
    "VOADOR", "PSIQUICO", "INSETO", "PEDRA", "FANTASMA", "DRAGAO", "SOMBRIO", "A\200O", "FADA" },
  { "", "ノーマル", "ほのお", "みず", "でんき", "くさ", "こおり", "かくとう", "どく", "じめん",
    "ひこう", "エスパー", "むし", "いわ", "ゴースト", "ドラゴン", "あく", "はがね", "フェアリー" },
  { "", "노말", "불꽃", "물", "전기", "풀", "얼음", "격투", "독", "땅",
    "비행", "에스퍼", "벌레", "바위", "고스트", "드래곤", "악", "강철", "페어리" },
};
#pragma endregion

const char *T(StrId id) { return STRINGS[gLang][id]; }
const char *medalName(int i)  { return MED_NAME[gLang][i]; }
const char *medalLabel(int i) { return MED_LBL[gLang][i]; }
const char *medalDesc(int i)  { return MED_DSC[gLang][i]; }

#pragma region Battle type label lookup
const char *typeName(uint8_t type) {
  if (type >= TYPE_NAME_COUNT) return "";
  return TYPE_NAMES[gLang][type];
}
#pragma endregion

void loadLang() {
  Preferences p;
  p.begin("tamapoke", true);  // read-only
  uint8_t v = p.getUChar("lang", LANG_DEFAULT);
  p.end();
  gLang = (v < LANG_COUNT) ? (Lang)v : LANG_DEFAULT;
}

void setLang(Lang l) {
  if (l >= LANG_COUNT) return;
  gLang = l;
  Preferences p;
  p.begin("tamapoke", false);
  p.putUChar("lang", (uint8_t)l);
  p.end();
}
