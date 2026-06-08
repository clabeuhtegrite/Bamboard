/* Bamboard showcase — self-hosted i18n (no third-party widget, matching the
   project's local-first ethos). The page ships in English (the DOM is the
   English source and the fallback); this script swaps marked text into the
   visitor's language when it is one Bamboard supports.

   Supported languages mirror the firmware (firmware/src/ui/i18n.cpp):
   English, Español, Français, Português, Deutsch — English is the fallback.

   Markup hooks in index.html:
     data-i18n="key"            -> element.textContent
     data-i18n-html="key"       -> element.innerHTML (value may contain markup)
     data-i18n-attr="a:key;b:k" -> sets attribute(s) a, b (e.g. alt, aria-label)
   Keys absent from a language's table fall back to the English DOM baseline.

   Language is chosen by: ?lang=xx (and remembered) > localStorage > the
   browser's Accept-Languages > English. The JSON block below is strict JSON
   (delimited by the i18n:json markers) so it can be validated in CI. */
(function () {
  "use strict";

  var LANGS = ["en", "es", "fr", "pt", "de"];
  var NAMES = { en: "English", es: "Español", fr: "Français", pt: "Português", de: "Deutsch" };

  var DICTS = /*i18n:json*/{
    "es": {
      "meta.title": "Bamboard — un monitor de escritorio táctil para tus impresoras Bambu Lab",
      "meta.desc": "Bamboard es un monitor de escritorio ESP32-S3 de código abierto (~20 €) con pantalla táctil de 4,3″ que muestra qué hacen tus impresoras Bambu Lab — en tiempo real, íntegramente a través de tu Bambuddy autoalojado. Fláshealo desde el navegador.",
      "nav.features": "Características", "nav.screens": "Pantallas", "nav.how": "Cómo funciona", "nav.hardware": "Hardware", "nav.install": "Instalar",
      "nav.flashIt": "Flashear", "nav.home": "Inicio de Bamboard", "nav.github": "Repositorio de GitHub", "nav.menu": "Alternar menú", "nav.primary": "Principal", "lang.label": "Idioma",
      "ui.latest": "Última", "hero.noSolder": "Sin soldadura",
      "hero.eyebrow": "Monitor de escritorio de código abierto",
      "hero.title": "Vigila cada impresión<br>de tu Bambu&nbsp;Lab",
      "hero.lede": "Un pequeño dispositivo ESP32-S3 con pantalla táctil de 4,3″ que vive en tu escritorio y muestra —en tiempo real— qué están haciendo tus impresoras. Habla <strong>solo</strong> con tu Bambuddy autoalojado, así que tus impresoras nunca ven una nube de terceros.",
      "hero.flashBrowser": "Fláshealo desde el navegador", "hero.viewGithub": "Ver en GitHub",
      "hero.imgAlt": "Bamboard en la carcasa impresa en 3D de TomE1337, con su pantalla de 4,3 pulgadas mostrando el panel en vivo durante una impresión",
      "feat.eyebrow": "Por qué Bamboard", "feat.title": "Una pantalla de estado de un vistazo, a tu manera",
      "feat.intro": "Todo lo que Bambuddy ya sabe de tu granja —temperaturas, progreso, AMS, cola, historial— en una pantalla dedicada que controlas de principio a fin.",
      "feat.local.h": "Local primero y privado", "feat.local.p": "Habla REST con tu propia instancia de Bambuddy. Tus impresoras se mantienen fuera de cualquier nube de terceros — tus datos nunca salen de tu red salvo que elijas HTTPS.",
      "feat.cheap.h": "Unos 20 €, sin soldadura", "feat.cheap.p": "Una placa ESP32-S3 todo en uno, un cable USB-C, cuatro tornillos y ~70 g de filamento para la carcasa. Todo en un solo PCB — nada que cablear.",
      "feat.ota.h": "Se actualiza solo", "feat.ota.p": "Fláshealo una vez desde el navegador; después cada versión llega por aire desde GitHub. El dispositivo se reinicia cada noche para recibir el nuevo firmware sin intervención.",
      "feat.touch.h": "Interfaz táctil ante todo", "feat.touch.p": "Seis pestañas, toca o desliza para cambiar. Pausa / Reanuda / Detén una impresión, alterna la luz del cajón o inicia un ciclo de secado del AMS — directamente desde el panel.",
      "feat.errors.h": "No deja escapar errores", "feat.errors.p": "Los fallos HMS aparecen en rojo con una alerta a pantalla completa que palpita y se rearma hasta resolverse. Los trabajos terminados o fallidos lanzan un aviso desde cualquier pestaña.",
      "feat.langs.h": "Cinco idiomas", "feat.langs.p": "Elige el idioma de la interfaz en el primer arranque — inglés, español, francés, portugués o alemán — sin reflashear para cambiarlo.",
      "scr.eyebrow": "Las pantallas", "scr.title": "Seis pestañas, todo de un vistazo",
      "scr.intro": "Renders en vivo desde un conjunto de datos de demostración integrado, regenerados en cada versión — así lo que ves aquí siempre coincide con el firmware actual.",
      "scr.tab.live.sub": "Temperaturas, progreso y controles", "scr.tab.ams.sub": "Ranuras, humedad y secado", "scr.tab.printers.sub": "Toda tu granja",
      "scr.tab.queue.sub": "Trabajos pendientes en orden", "scr.tab.history.sub": "Estadísticas e impresiones recientes", "scr.tab.settings.sub": "Diagnóstico y brillo",
      "scr.alt.live": "Panel en vivo durante una impresión — temperaturas de boquilla, cama y cámara, anillo de progreso, ETA, miniatura de la cámara y controles Pausa / Detener / velocidad",
      "scr.alt.ams": "Vista del AMS — color, tipo y porcentaje restante de filamento por ranura, humedad, temperatura y una cuenta atrás de secado en vivo",
      "scr.alt.printers": "Lista de impresoras — cada impresora que Bambuddy conoce, con temperaturas y ETA en línea en las que están imprimiendo",
      "scr.alt.queue": "Cola de impresión — trabajos pendientes en orden, cada uno con su impresora de destino",
      "scr.alt.history": "Historial y estadísticas — tasa de éxito, filamento total, tiempo total y las últimas impresiones",
      "scr.alt.settings": "Ajustes — URL de Bambuddy y versión del servidor, diagnóstico del dispositivo, un selector de brillo y restablecimiento de fábrica",
      "scr.desc.live": "<b>Panel en vivo.</b> Temperaturas de boquilla / cama / cámara, progreso por capas, ETA y nombre del archivo — más una miniatura de la cámara y Pausa / Reanudar / Detener en línea, un interruptor de luz, el selector de velocidad y la lectura del ventilador durante la impresión.",
      "scr.desc.ams": "<b>Vista del AMS.</b> Color, tipo y porcentaje restante de filamento por ranura, con humedad, temperatura y una cuenta atrás de secado en vivo del AMS. El filamento transparente muestra un damero; un botón Secar / Detener controla las unidades con calentador a la propia temperatura del filamento cargado.",
      "scr.desc.printers": "<b>Lista de impresoras.</b> Cada impresora que Bambuddy conoce, resaltada por estado. Las filas que están imprimiendo llevan temperaturas y ETA en línea; toca una para enfocarla y saltar a En vivo.",
      "scr.desc.queue": "<b>Cola de impresión.</b> Los trabajos que Bambuddy aún tiene pendientes, en orden, cada uno etiquetado con su impresora de destino.",
      "scr.desc.history": "<b>Historial y estadísticas.</b> Las últimas impresiones junto a indicadores de por vida — tasa de éxito, filamento total y tiempo total de impresión.",
      "scr.desc.settings": "<b>Ajustes.</b> URL de Bambuddy y la versión + tiempo activo del servidor, IP local, RSSI Wi-Fi y tiempo activo del dispositivo, un selector de brillo 1–5, un botón Wi-Fi (reabre el portal cautivo sin borrar nada) y un restablecimiento de fábrica con doble toque.",
      "how.eyebrow": "Cómo funciona", "how.title": "Tus impresoras → Bambuddy → Bamboard",
      "how.intro": "Bamboard es un cliente fino y dedicado para la API de Bambuddy. Sin puente, sin servicio extra que ejecutar más allá del Bambuddy que ya alojas.",
      "how.node.printers": "impresoras", "how.node.selfhosted": "autoalojado", "how.node.device": "el dispositivo",
      "how.note.h": "HTTP en la LAN, o HTTPS a un dominio", "how.note.p": "validado contra un paquete de CA refrescado por CI · token opcional de Cloudflare Access",
      "how.rt.h": "Estado en vivo.", "how.rt.p": "Sondea la API REST de Bambuddy cada 2 s para cada impresora (<code>printer_status</code>): temperaturas, progreso y alertas HMS llegan en pocos segundos — solo autenticación por clave API, nada que iniciar sesión.",
      "how.lan.h": "LAN o remoto, tú decides.", "how.lan.p": "HTTP simple a una IP privada / <code>.local</code> en tu LAN por defecto, o HTTPS a un dominio con el certificado del servidor validado contra un paquete de CA raíz integrado y refrescado por CI.",
      "how.cf.h": "Listo para Cloudflare Access.", "how.cf.p": "Campos opcionales de token de servicio te permiten alcanzar un Bambuddy publicado tras Cloudflare Access — las cabeceras viajan solo por HTTPS / WSS, nunca en texto claro.",
      "how.setup.h": "Configuración sin reflashear.", "how.setup.p": "Un portal cautivo en el primer arranque recoge el Wi-Fi, la URL + clave API de Bambuddy, la zona horaria, la hora del reinicio diario y el idioma. Mantén pulsado <kbd>BOOT</kbd> al encender para ejecutarlo de nuevo.",
      "hw.eyebrow": "Hardware", "hw.title": "Una placa. Sin cables. Sin soldadura.",
      "hw.intro": "Sin cableado de DevKit, sin placas de expansión. Una sola Guition todo en uno encaja en una carcasa imprimible.",
      "hw.spec.board": "<b>Guition JC4827W543</b> — ESP32-S3-WROOM-1 con 8&nbsp;MB de PSRAM, todo en un PCB.",
      "hw.spec.panel": "<b>IPS de 4,3″, 480 × 272</b> panel RGB-paralelo con capa táctil capacitiva GT911.",
      "hw.spec.case": "<b>Carcasa imprimible en FDM</b> — un diseño comunitario de 3 partes de <a href=\"https://www.printables.com/@TomE1337_2178164\" target=\"_blank\" rel=\"noopener\">TomE1337</a> (CC BY-NC-SA): cuerpo + tapa + soporte de escritorio. ~120&nbsp;g de filamento.",
      "hw.spec.usb": "<b>Solo un cable USB-C</b> para alimentación y el flasheo único — nada más que conectar.",
      "hw.bomBtn": "Lista de materiales", "hw.bom.total": "lista de materiales total",
      "hw.bom.board": "Placa todo en uno", "hw.bom.usb": "Cable de datos USB-C", "hw.bom.fasteners": "Tornillos de la carcasa", "hw.bom.fastenersV": "según modelo", "hw.bom.fil": "Filamento (carcasa)", "hw.bom.solder": "Soldadura", "hw.bom.solderV": "ninguna",
      "inst.eyebrow": "Instalación y actualizaciones", "inst.title": "Fláshealo directamente desde esta página",
      "inst.intro": "La primera instalación se ejecuta en el navegador por Web Serial — sin PlatformIO, sin CLI, sin toolchain. Todo lo demás llega por aire.",
      "inst.browser.h": "Instalación por navegador", "inst.browser.p": "Chrome, Edge u Opera en un ordenador. Conecta la placa a un cable de datos USB-C y haz clic en Conectar.",
      "inst.unsupported": "⚠️ Este navegador no puede comunicarse con dispositivos serie USB. Abre esta página en <strong>Chrome, Edge u Opera en un ordenador</strong> para flashear.",
      "inst.notallowed": "⚠️ Web Serial necesita una página segura (https).",
      "inst.placeholder": "⚠️ El manifiesto publicado es un marcador de posición (<code>0.0.0-dev</code>) — el CI de la versión aún no se ha ejecutado. Usa la <a href=\"https://github.com/clabeuhtegrite/Bamboard/releases/latest\" target=\"_blank\" rel=\"noopener\">última versión</a> en GitHub en su lugar.",
      "inst.step1": "Usa <b>Chrome, Edge u Opera</b> en un ordenador (Web Serial no está en Firefox, Safari ni en móviles).",
      "inst.step2": "Conecta la placa a un cable USB-C de <b>datos</b> (no solo de carga).",
      "inst.step3": "Haz clic en <b>Conectar</b> y elige el puerto serie que aparezca.",
      "inst.step4": "¿Sin puerto? Entra en modo descarga: mantén <kbd>BOOT</kbd>, pulsa <kbd>RST</kbd>, suelta <kbd>BOOT</kbd>, reintenta.",
      "inst.step5": "En el primer arranque, únete al Wi-Fi <code>Bamboard-setup</code> y termina la configuración en el portal cautivo.",
      "inst.note": "Solo lo haces una vez. Tras el primer flasheo, el dispositivo se actualiza por aire desde GitHub Releases en cada arranque.",
      "inst.other.h": "Otros navegadores y flasheo manual",
      "inst.other.p": "Web Serial es solo para Chromium en escritorio. En Firefox, Safari, Linux sin un navegador Chromium, o para flashear a mano, descarga <code>bamboard-factory.bin</code> de la <a href=\"https://github.com/clabeuhtegrite/Bamboard/releases/latest\" target=\"_blank\" rel=\"noopener\">última versión</a> y escríbelo en el offset <code>0x0</code> con <a href=\"https://github.com/espressif/esptool\" target=\"_blank\" rel=\"noopener\">esptool</a>:",
      "inst.copy": "Copiar comando",
      "cta.eyebrow": "Empezar", "cta.title": "Construye tu propio Bamboard",
      "cta.p": "Pide la placa de ~20 €, imprime la carcasa y fláshealo desde el navegador. La documentación te guía por el montaje, el flasheo y la configuración.",
      "cta.flashNow": "Fláshealo ahora", "cta.readDocs": "Leer la documentación",
      "foot.tagline": "Un monitor de escritorio táctil de código abierto para impresoras Bambu Lab, con Bambuddy autoalojado.",
      "foot.h.project": "Proyecto", "foot.h.docs": "Documentación", "foot.h.related": "Relacionado",
      "foot.repo": "Repositorio de GitHub", "foot.releases": "Versiones", "foot.changelog": "Registro de cambios", "foot.license": "Licencia MIT",
      "foot.assembly": "Montaje", "foot.flashing": "Flasheo", "foot.config": "Configuración", "foot.bom": "Lista de materiales",
      "foot.bottom1": "Bajo licencia MIT · Sin afiliación con Bambu Lab ni con el proyecto Bambuddy.", "foot.bottom2": "Hecho para makers que autoalojan.",
      "lb.label": "Captura ampliada", "lb.close": "Cerrar"
    },
    "fr": {
      "meta.title": "Bamboard — un moniteur de bureau tactile pour vos imprimantes Bambu Lab",
      "meta.desc": "Bamboard est un moniteur de bureau ESP32-S3 open-source (~20 €) avec un écran tactile de 4,3″ qui montre ce que font vos imprimantes Bambu Lab — en temps réel, entièrement via votre Bambuddy auto-hébergé. Flashez-le depuis le navigateur.",
      "nav.features": "Fonctionnalités", "nav.screens": "Écrans", "nav.how": "Fonctionnement", "nav.hardware": "Matériel", "nav.install": "Installer",
      "nav.flashIt": "Flasher", "nav.home": "Accueil Bamboard", "nav.github": "Dépôt GitHub", "nav.menu": "Basculer le menu", "nav.primary": "Principal", "lang.label": "Langue",
      "ui.latest": "Dernière", "hero.noSolder": "Sans soudure",
      "hero.eyebrow": "Moniteur de bureau open-source",
      "hero.title": "Gardez un œil sur chaque<br>impression Bambu&nbsp;Lab",
      "hero.lede": "Un petit appareil ESP32-S3 avec un écran tactile de 4,3″ qui trône sur votre bureau et montre — en temps réel — ce que font vos imprimantes. Il communique <strong>uniquement</strong> avec votre Bambuddy auto-hébergé, donc vos imprimantes ne voient jamais de cloud tiers.",
      "hero.flashBrowser": "Flasher depuis le navigateur", "hero.viewGithub": "Voir sur GitHub",
      "hero.imgAlt": "Bamboard dans le boîtier imprimé en 3D de TomE1337, son écran de 4,3 pouces affichant le tableau de bord en direct pendant une impression",
      "feat.eyebrow": "Pourquoi Bamboard", "feat.title": "Un écran d'état consultable d'un coup d'œil, à votre manière",
      "feat.intro": "Tout ce que Bambuddy sait déjà de votre parc — températures, progression, AMS, file, historique — sur un écran dédié que vous maîtrisez de bout en bout.",
      "feat.local.h": "Local d'abord et privé", "feat.local.p": "Parle REST à votre propre instance Bambuddy. Vos imprimantes restent hors de tout cloud tiers — vos données ne quittent jamais votre réseau, sauf si vous choisissez HTTPS.",
      "feat.cheap.h": "Environ 20 €, sans soudure", "feat.cheap.p": "Une carte ESP32-S3 tout-en-un, un câble USB-C, quatre vis et ~70 g de filament pour le boîtier. Tout est sur un seul PCB — rien à câbler.",
      "feat.ota.h": "Se met à jour tout seul", "feat.ota.p": "Flashez une fois depuis le navigateur ; ensuite chaque version arrive par les airs depuis GitHub. L'appareil redémarre chaque nuit pour recevoir le nouveau firmware sans intervention.",
      "feat.touch.h": "Interface pensée pour le tactile", "feat.touch.p": "Six onglets, touchez ou glissez pour changer. Pause / Reprise / Arrêt d'une impression, bascule de la lumière du caisson ou lancement d'un cycle de séchage AMS — directement depuis le panneau.",
      "feat.errors.h": "Ne laisse pas filer les erreurs", "feat.errors.p": "Les défauts HMS s'affichent en rouge avec une alerte plein écran pulsante qui se réarme jusqu'à résolution. Les travaux terminés ou échoués déclenchent une notification depuis n'importe quel onglet.",
      "feat.langs.h": "Cinq langues", "feat.langs.p": "Choisissez la langue de l'interface au premier démarrage — anglais, espagnol, français, portugais ou allemand — sans reflasher pour en changer.",
      "scr.eyebrow": "Les écrans", "scr.title": "Six onglets, tout d'un coup d'œil",
      "scr.intro": "Rendus en direct depuis un jeu de données de démo intégré, régénérés à chaque version — ce que vous voyez ici correspond donc toujours au firmware actuel.",
      "scr.tab.live.sub": "Températures, progression et contrôles", "scr.tab.ams.sub": "Emplacements, humidité et séchage", "scr.tab.printers.sub": "Tout votre parc",
      "scr.tab.queue.sub": "Travaux en attente, dans l'ordre", "scr.tab.history.sub": "Statistiques et impressions récentes", "scr.tab.settings.sub": "Diagnostics et luminosité",
      "scr.alt.live": "Tableau de bord en direct pendant une impression — températures buse, plateau et caisson, anneau de progression, ETA, vignette caméra et contrôles Pause / Arrêt / vitesse",
      "scr.alt.ams": "Vue AMS — couleur, type et pourcentage restant du filament par emplacement, humidité, température et un compte à rebours de séchage en direct",
      "scr.alt.printers": "Liste des imprimantes — chaque imprimante connue de Bambuddy, avec températures et ETA en ligne sur celles qui impriment",
      "scr.alt.queue": "File d'impression — travaux en attente dans l'ordre, chacun avec son imprimante cible",
      "scr.alt.history": "Historique et statistiques — taux de réussite, filament total, temps total et les dernières impressions",
      "scr.alt.settings": "Réglages — URL Bambuddy et version du serveur, diagnostics de l'appareil, un sélecteur de luminosité et la réinitialisation d'usine",
      "scr.desc.live": "<b>Tableau de bord en direct.</b> Températures buse / plateau / caisson, progression par couches, ETA et nom de fichier — plus une vignette caméra et Pause / Reprise / Arrêt en ligne, une bascule de lumière, le sélecteur de vitesse et la lecture du ventilateur pendant l'impression.",
      "scr.desc.ams": "<b>Vue AMS.</b> Couleur, type et pourcentage restant du filament par emplacement, avec humidité, température et un compte à rebours de séchage AMS en direct. Le filament transparent affiche un damier ; un bouton Sécher / Arrêt pilote les unités chauffantes à la température propre du filament chargé.",
      "scr.desc.printers": "<b>Liste des imprimantes.</b> Chaque imprimante connue de Bambuddy, mise en évidence par état. Les lignes en cours d'impression portent températures et ETA en ligne ; touchez-en une pour la cibler et passer à Direct.",
      "scr.desc.queue": "<b>File d'impression.</b> Les travaux que Bambuddy garde encore en attente, dans l'ordre, chacun étiqueté avec son imprimante cible.",
      "scr.desc.history": "<b>Historique et statistiques.</b> Les dernières impressions aux côtés d'indicateurs cumulés — taux de réussite, filament total et temps d'impression total.",
      "scr.desc.settings": "<b>Réglages.</b> URL Bambuddy et version + temps de fonctionnement du serveur, IP locale, RSSI Wi-Fi et temps de fonctionnement de l'appareil, un sélecteur de luminosité 1–5, un bouton Wi-Fi (rouvre le portail captif sans rien effacer) et une réinitialisation d'usine en deux touches.",
      "how.eyebrow": "Fonctionnement", "how.title": "Vos imprimantes → Bambuddy → Bamboard",
      "how.intro": "Bamboard est un client fin et dédié pour l'API Bambuddy. Pas de passerelle, pas de service supplémentaire à faire tourner au-delà du Bambuddy que vous hébergez déjà.",
      "how.node.printers": "imprimantes", "how.node.selfhosted": "auto-hébergé", "how.node.device": "l'appareil",
      "how.note.h": "HTTP sur le LAN, ou HTTPS vers un domaine", "how.note.p": "validé contre un paquet de CA rafraîchi par CI · jeton Cloudflare Access optionnel",
      "how.rt.h": "Statut en direct.", "how.rt.p": "Interroge l'API REST de Bambuddy toutes les 2 s pour chaque imprimante (<code>printer_status</code>) : températures, progression et alertes HMS arrivent en quelques secondes — authentification par clé API uniquement, rien où se connecter.",
      "how.lan.h": "LAN ou distant, à vous de voir.", "how.lan.p": "HTTP simple vers une IP privée / <code>.local</code> sur votre LAN par défaut, ou HTTPS vers un domaine avec le certificat du serveur validé contre un paquet de CA racine intégré et rafraîchi par CI.",
      "how.cf.h": "Prêt pour Cloudflare Access.", "how.cf.p": "Des champs de jeton de service optionnels vous permettent d'atteindre un Bambuddy publié derrière Cloudflare Access — les en-têtes ne circulent que sur HTTPS / WSS, jamais en clair.",
      "how.setup.h": "Configuration sans reflashage.", "how.setup.p": "Un portail captif au premier démarrage recueille le Wi-Fi, l'URL + clé API Bambuddy, le fuseau horaire, l'heure du redémarrage quotidien et la langue. Maintenez <kbd>BOOT</kbd> à la mise sous tension pour le relancer.",
      "hw.eyebrow": "Matériel", "hw.title": "Une carte. Aucun câblage. Aucune soudure.",
      "hw.intro": "Pas de câblage de DevKit, pas de cartes d'extension. Une seule Guition tout-en-un se glisse dans un boîtier imprimable.",
      "hw.spec.board": "<b>Guition JC4827W543</b> — ESP32-S3-WROOM-1 avec 8&nbsp;Mo de PSRAM, le tout sur un seul PCB.",
      "hw.spec.panel": "<b>IPS 4,3″, 480 × 272</b> dalle RGB-parallèle avec une couche tactile capacitive GT911.",
      "hw.spec.case": "<b>Boîtier imprimable en FDM</b> — un design communautaire en 3 parties par <a href=\"https://www.printables.com/@TomE1337_2178164\" target=\"_blank\" rel=\"noopener\">TomE1337</a> (CC BY-NC-SA) : corps + capot + support de bureau. ~120&nbsp;g de filament.",
      "hw.spec.usb": "<b>Juste un câble USB-C</b> pour l'alimentation et le flashage unique — rien d'autre à brancher.",
      "hw.bomBtn": "Nomenclature", "hw.bom.total": "nomenclature totale",
      "hw.bom.board": "Carte tout-en-un", "hw.bom.usb": "Câble de données USB-C", "hw.bom.fasteners": "Vis du boîtier", "hw.bom.fastenersV": "selon le modèle", "hw.bom.fil": "Filament (boîtier)", "hw.bom.solder": "Soudure", "hw.bom.solderV": "aucune",
      "inst.eyebrow": "Installation et mises à jour", "inst.title": "Flashez-le directement depuis cette page",
      "inst.intro": "La première installation s'exécute dans le navigateur via Web Serial — pas de PlatformIO, pas de CLI, pas de toolchain. Tout le reste arrive par les airs.",
      "inst.browser.h": "Installation par navigateur", "inst.browser.p": "Chrome, Edge ou Opera sur un ordinateur. Branchez la carte à un câble de données USB-C et cliquez sur Connecter.",
      "inst.unsupported": "⚠️ Ce navigateur ne peut pas dialoguer avec les périphériques série USB. Ouvrez cette page dans <strong>Chrome, Edge ou Opera sur un ordinateur</strong> pour flasher.",
      "inst.notallowed": "⚠️ Web Serial nécessite une page sécurisée (https).",
      "inst.placeholder": "⚠️ Le manifeste publié est un espace réservé (<code>0.0.0-dev</code>) — le CI de version ne s'est pas encore exécuté. Utilisez plutôt la <a href=\"https://github.com/clabeuhtegrite/Bamboard/releases/latest\" target=\"_blank\" rel=\"noopener\">dernière version</a> sur GitHub.",
      "inst.step1": "Utilisez <b>Chrome, Edge ou Opera</b> sur un ordinateur (Web Serial n'est ni dans Firefox, ni Safari, ni sur téléphone).",
      "inst.step2": "Branchez la carte à un câble USB-C de <b>données</b> (pas seulement de charge).",
      "inst.step3": "Cliquez sur <b>Connecter</b> et choisissez le port série qui apparaît.",
      "inst.step4": "Pas de port ? Passez en mode téléchargement : maintenez <kbd>BOOT</kbd>, appuyez sur <kbd>RST</kbd>, relâchez <kbd>BOOT</kbd>, recommencez.",
      "inst.step5": "Au premier démarrage, rejoignez le Wi-Fi <code>Bamboard-setup</code> et terminez la configuration dans le portail captif.",
      "inst.note": "Vous ne le faites qu'une fois. Après le premier flashage, l'appareil se met à jour par les airs depuis GitHub Releases à chaque démarrage.",
      "inst.other.h": "Autres navigateurs et flashage manuel",
      "inst.other.p": "Web Serial est réservé à Chromium sur ordinateur. Sur Firefox, Safari, Linux sans navigateur Chromium, ou pour flasher à la main, téléchargez <code>bamboard-factory.bin</code> depuis la <a href=\"https://github.com/clabeuhtegrite/Bamboard/releases/latest\" target=\"_blank\" rel=\"noopener\">dernière version</a> et écrivez-le à l'offset <code>0x0</code> avec <a href=\"https://github.com/espressif/esptool\" target=\"_blank\" rel=\"noopener\">esptool</a> :",
      "inst.copy": "Copier la commande",
      "cta.eyebrow": "Commencer", "cta.title": "Construisez votre propre Bamboard",
      "cta.p": "Commandez la carte à ~20 €, imprimez le boîtier et flashez-le depuis votre navigateur. La documentation vous guide pour l'assemblage, le flashage et la configuration.",
      "cta.flashNow": "Flasher maintenant", "cta.readDocs": "Lire la documentation",
      "foot.tagline": "Un moniteur de bureau tactile open-source pour imprimantes Bambu Lab, propulsé par Bambuddy auto-hébergé.",
      "foot.h.project": "Projet", "foot.h.docs": "Documentation", "foot.h.related": "Liés",
      "foot.repo": "Dépôt GitHub", "foot.releases": "Versions", "foot.changelog": "Journal des modifications", "foot.license": "Licence MIT",
      "foot.assembly": "Assemblage", "foot.flashing": "Flashage", "foot.config": "Configuration", "foot.bom": "Nomenclature",
      "foot.bottom1": "Sous licence MIT · Sans affiliation avec Bambu Lab ni le projet Bambuddy.", "foot.bottom2": "Conçu pour les makers qui auto-hébergent.",
      "lb.label": "Capture agrandie", "lb.close": "Fermer"
    },
    "pt": {
      "meta.title": "Bamboard — um monitor de secretária tátil para as suas impressoras Bambu Lab",
      "meta.desc": "O Bamboard é um monitor de secretária ESP32-S3 de código aberto (~20 €) com ecrã tátil de 4,3″ que mostra o que as suas impressoras Bambu Lab estão a fazer — em tempo real, inteiramente através do seu Bambuddy auto-hospedado. Grave-o a partir do navegador.",
      "nav.features": "Funcionalidades", "nav.screens": "Ecrãs", "nav.how": "Como funciona", "nav.hardware": "Hardware", "nav.install": "Instalar",
      "nav.flashIt": "Gravar", "nav.home": "Início do Bamboard", "nav.github": "Repositório GitHub", "nav.menu": "Alternar menu", "nav.primary": "Principal", "lang.label": "Idioma",
      "ui.latest": "Última", "hero.noSolder": "Sem solda",
      "hero.eyebrow": "Monitor de secretária de código aberto",
      "hero.title": "Fique de olho em cada<br>impressão Bambu&nbsp;Lab",
      "hero.lede": "Um pequeno dispositivo ESP32-S3 com ecrã tátil de 4,3″ que fica na sua secretária e mostra — em tempo real — o que as suas impressoras estão a fazer. Comunica <strong>apenas</strong> com o seu Bambuddy auto-hospedado, por isso as suas impressoras nunca veem uma nuvem de terceiros.",
      "hero.flashBrowser": "Gravar a partir do navegador", "hero.viewGithub": "Ver no GitHub",
      "hero.imgAlt": "Bamboard na caixa impressa em 3D de TomE1337, com o seu ecrã de 4,3 polegadas a mostrar o painel ao vivo durante uma impressão",
      "feat.eyebrow": "Porquê o Bamboard", "feat.title": "Um ecrã de estado consultável num relance, à sua maneira",
      "feat.intro": "Tudo o que o Bambuddy já sabe sobre o seu parque — temperaturas, progresso, AMS, fila, histórico — num ecrã dedicado que controla de ponta a ponta.",
      "feat.local.h": "Local primeiro e privado", "feat.local.p": "Fala REST com a sua própria instância Bambuddy. As suas impressoras ficam fora de qualquer nuvem de terceiros — os seus dados nunca saem da sua rede a menos que escolha HTTPS.",
      "feat.cheap.h": "Cerca de 20 €, sem solda", "feat.cheap.p": "Uma placa ESP32-S3 tudo-em-um, um cabo USB-C, quatro parafusos e ~70 g de filamento para a caixa. Tudo num único PCB — nada para ligar.",
      "feat.ota.h": "Atualiza-se sozinho", "feat.ota.p": "Grave uma vez a partir do navegador; depois cada versão chega por via aérea a partir do GitHub. O dispositivo reinicia todas as noites para receber o novo firmware sem intervenção.",
      "feat.touch.h": "Interface tátil em primeiro lugar", "feat.touch.p": "Seis separadores, toque ou deslize para mudar. Pausar / Retomar / Parar uma impressão, alternar a luz da câmara ou iniciar um ciclo de secagem do AMS — diretamente do painel.",
      "feat.errors.h": "Não deixa escapar erros", "feat.errors.p": "As falhas HMS surgem a vermelho com um alerta de ecrã inteiro pulsante que se rearma até ser resolvido. Trabalhos concluídos ou falhados lançam um aviso a partir de qualquer separador.",
      "feat.langs.h": "Cinco idiomas", "feat.langs.p": "Escolha o idioma da interface no primeiro arranque — inglês, espanhol, francês, português ou alemão — sem regravar para mudar.",
      "scr.eyebrow": "Os ecrãs", "scr.title": "Seis separadores, tudo num relance",
      "scr.intro": "Renders ao vivo de um conjunto de dados de demonstração integrado, regenerados em cada versão — por isso o que vê aqui corresponde sempre ao firmware atual.",
      "scr.tab.live.sub": "Temperaturas, progresso e controlos", "scr.tab.ams.sub": "Ranhuras, humidade e secagem", "scr.tab.printers.sub": "Todo o seu parque",
      "scr.tab.queue.sub": "Trabalhos pendentes por ordem", "scr.tab.history.sub": "Estatísticas e impressões recentes", "scr.tab.settings.sub": "Diagnósticos e brilho",
      "scr.alt.live": "Painel ao vivo durante uma impressão — temperaturas de bico, mesa e câmara, anel de progresso, ETA, miniatura da câmara e controlos Pausar / Parar / velocidade",
      "scr.alt.ams": "Vista do AMS — cor, tipo e percentagem restante do filamento por ranhura, humidade, temperatura e uma contagem decrescente de secagem ao vivo",
      "scr.alt.printers": "Lista de impressoras — cada impressora que o Bambuddy conhece, com temperaturas e ETA em linha nas que estão a imprimir",
      "scr.alt.queue": "Fila de impressão — trabalhos pendentes por ordem, cada um com a sua impressora de destino",
      "scr.alt.history": "Histórico e estatísticas — taxa de sucesso, filamento total, tempo total e as últimas impressões",
      "scr.alt.settings": "Definições — URL do Bambuddy e versão do servidor, diagnósticos do dispositivo, um seletor de brilho e reposição de fábrica",
      "scr.desc.live": "<b>Painel ao vivo.</b> Temperaturas de bico / mesa / câmara, progresso por camadas, ETA e nome do ficheiro — mais uma miniatura da câmara e Pausar / Retomar / Parar em linha, um interruptor de luz, o seletor de velocidade e a leitura da ventoinha durante a impressão.",
      "scr.desc.ams": "<b>Vista do AMS.</b> Cor, tipo e percentagem restante do filamento por ranhura, com humidade, temperatura e uma contagem decrescente de secagem do AMS ao vivo. O filamento transparente mostra um xadrez; um botão Secar / Parar controla as unidades com aquecedor à própria temperatura do filamento carregado.",
      "scr.desc.printers": "<b>Lista de impressoras.</b> Cada impressora que o Bambuddy conhece, destacada por estado. As linhas que estão a imprimir levam temperaturas e ETA em linha; toque numa para a focar e saltar para Ao vivo.",
      "scr.desc.queue": "<b>Fila de impressão.</b> Os trabalhos que o Bambuddy ainda tem pendentes, por ordem, cada um etiquetado com a sua impressora de destino.",
      "scr.desc.history": "<b>Histórico e estatísticas.</b> As últimas impressões ao lado de indicadores acumulados — taxa de sucesso, filamento total e tempo total de impressão.",
      "scr.desc.settings": "<b>Definições.</b> URL do Bambuddy e a versão + tempo de atividade do servidor, IP local, RSSI Wi-Fi e tempo de atividade do dispositivo, um seletor de brilho 1–5, um botão Wi-Fi (reabre o portal cativo sem apagar nada) e uma reposição de fábrica com duplo toque.",
      "how.eyebrow": "Como funciona", "how.title": "As suas impressoras → Bambuddy → Bamboard",
      "how.intro": "O Bamboard é um cliente fino e dedicado para a API do Bambuddy. Sem ponte, sem serviço extra para executar além do Bambuddy que já aloja.",
      "how.node.printers": "impressoras", "how.node.selfhosted": "auto-hospedado", "how.node.device": "o dispositivo",
      "how.note.h": "HTTP na LAN, ou HTTPS para um domínio", "how.note.p": "validado contra um pacote de CA atualizado por CI · token Cloudflare Access opcional",
      "how.rt.h": "Estado ao vivo.", "how.rt.p": "Consulta a API REST do Bambuddy a cada 2 s para cada impressora (<code>printer_status</code>): temperaturas, progresso e alertas HMS chegam em poucos segundos — apenas autenticação por chave API, nada para iniciar sessão.",
      "how.lan.h": "LAN ou remoto, você decide.", "how.lan.p": "HTTP simples para um IP privado / <code>.local</code> na sua LAN por defeito, ou HTTPS para um domínio com o certificado do servidor validado contra um pacote de CA raiz integrado e atualizado por CI.",
      "how.cf.h": "Pronto para Cloudflare Access.", "how.cf.p": "Campos opcionais de token de serviço permitem alcançar um Bambuddy publicado atrás do Cloudflare Access — os cabeçalhos viajam apenas por HTTPS / WSS, nunca em texto claro.",
      "how.setup.h": "Configuração sem regravar.", "how.setup.p": "Um portal cativo no primeiro arranque recolhe o Wi-Fi, o URL + chave API do Bambuddy, o fuso horário, a hora do reinício diário e o idioma. Mantenha <kbd>BOOT</kbd> ao ligar para o executar de novo.",
      "hw.eyebrow": "Hardware", "hw.title": "Uma placa. Sem cabos. Sem solda.",
      "hw.intro": "Sem cablagem de DevKit, sem placas de expansão. Uma única Guition tudo-em-um encaixa numa caixa imprimível.",
      "hw.spec.board": "<b>Guition JC4827W543</b> — ESP32-S3-WROOM-1 com 8&nbsp;MB de PSRAM, tudo num PCB.",
      "hw.spec.panel": "<b>IPS de 4,3″, 480 × 272</b> painel RGB-paralelo com uma camada tátil capacitiva GT911.",
      "hw.spec.case": "<b>Caixa imprimível em FDM</b> — um design comunitário de 3 partes por <a href=\"https://www.printables.com/@TomE1337_2178164\" target=\"_blank\" rel=\"noopener\">TomE1337</a> (CC BY-NC-SA): corpo + tampa + suporte de secretária. ~120&nbsp;g de filamento.",
      "hw.spec.usb": "<b>Apenas um cabo USB-C</b> para alimentação e a gravação única — nada mais para ligar.",
      "hw.bomBtn": "Lista de materiais", "hw.bom.total": "lista de materiais total",
      "hw.bom.board": "Placa tudo-em-um", "hw.bom.usb": "Cabo de dados USB-C", "hw.bom.fasteners": "Parafusos da caixa", "hw.bom.fastenersV": "conforme o modelo", "hw.bom.fil": "Filamento (caixa)", "hw.bom.solder": "Solda", "hw.bom.solderV": "nenhuma",
      "inst.eyebrow": "Instalação e atualizações", "inst.title": "Grave-o diretamente a partir desta página",
      "inst.intro": "A primeira instalação corre no navegador via Web Serial — sem PlatformIO, sem CLI, sem toolchain. Tudo o resto chega por via aérea.",
      "inst.browser.h": "Instalação pelo navegador", "inst.browser.p": "Chrome, Edge ou Opera num computador. Ligue a placa a um cabo de dados USB-C e clique em Ligar.",
      "inst.unsupported": "⚠️ Este navegador não consegue comunicar com dispositivos série USB. Abra esta página no <strong>Chrome, Edge ou Opera num computador</strong> para gravar.",
      "inst.notallowed": "⚠️ O Web Serial precisa de uma página segura (https).",
      "inst.placeholder": "⚠️ O manifesto publicado é um marcador de posição (<code>0.0.0-dev</code>) — o CI de versão ainda não correu. Use a <a href=\"https://github.com/clabeuhtegrite/Bamboard/releases/latest\" target=\"_blank\" rel=\"noopener\">última versão</a> no GitHub.",
      "inst.step1": "Use <b>Chrome, Edge ou Opera</b> num computador (o Web Serial não existe no Firefox, Safari ou em telemóveis).",
      "inst.step2": "Ligue a placa a um cabo USB-C de <b>dados</b> (não apenas de carga).",
      "inst.step3": "Clique em <b>Ligar</b> e escolha a porta série que aparece.",
      "inst.step4": "Sem porta? Entre em modo de download: mantenha <kbd>BOOT</kbd>, toque em <kbd>RST</kbd>, solte <kbd>BOOT</kbd>, tente de novo.",
      "inst.step5": "No primeiro arranque, ligue-se ao Wi-Fi <code>Bamboard-setup</code> e termine a configuração no portal cativo.",
      "inst.note": "Só faz isto uma vez. Após a primeira gravação, o dispositivo atualiza-se por via aérea a partir do GitHub Releases em cada arranque.",
      "inst.other.h": "Outros navegadores e gravação manual",
      "inst.other.p": "O Web Serial é só para Chromium em computador. No Firefox, Safari, Linux sem um navegador Chromium, ou para gravar à mão, descarregue <code>bamboard-factory.bin</code> da <a href=\"https://github.com/clabeuhtegrite/Bamboard/releases/latest\" target=\"_blank\" rel=\"noopener\">última versão</a> e escreva-o no offset <code>0x0</code> com <a href=\"https://github.com/espressif/esptool\" target=\"_blank\" rel=\"noopener\">esptool</a>:",
      "inst.copy": "Copiar comando",
      "cta.eyebrow": "Começar", "cta.title": "Construa o seu próprio Bamboard",
      "cta.p": "Encomende a placa de ~20 €, imprima a caixa e grave-o a partir do navegador. A documentação guia-o pela montagem, gravação e configuração.",
      "cta.flashNow": "Gravar agora", "cta.readDocs": "Ler a documentação",
      "foot.tagline": "Um monitor de secretária tátil de código aberto para impressoras Bambu Lab, com Bambuddy auto-hospedado.",
      "foot.h.project": "Projeto", "foot.h.docs": "Documentação", "foot.h.related": "Relacionados",
      "foot.repo": "Repositório GitHub", "foot.releases": "Versões", "foot.changelog": "Registo de alterações", "foot.license": "Licença MIT",
      "foot.assembly": "Montagem", "foot.flashing": "Gravação", "foot.config": "Configuração", "foot.bom": "Lista de materiais",
      "foot.bottom1": "Sob licença MIT · Sem afiliação com a Bambu Lab nem com o projeto Bambuddy.", "foot.bottom2": "Feito para makers que auto-hospedam.",
      "lb.label": "Captura ampliada", "lb.close": "Fechar"
    },
    "de": {
      "meta.title": "Bamboard — ein Touch-Schreibtischmonitor für deine Bambu-Lab-Drucker",
      "meta.desc": "Bamboard ist ein quelloffener ESP32-S3-Schreibtischmonitor (~20 €) mit 4,3″-Touchscreen, der in Echtzeit zeigt, was deine Bambu-Lab-Drucker tun — vollständig über dein selbstgehostetes Bambuddy. Flashe ihn direkt im Browser.",
      "nav.features": "Funktionen", "nav.screens": "Bildschirme", "nav.how": "So funktioniert's", "nav.hardware": "Hardware", "nav.install": "Installieren",
      "nav.flashIt": "Flashen", "nav.home": "Bamboard Startseite", "nav.github": "GitHub-Repository", "nav.menu": "Menü umschalten", "nav.primary": "Haupt", "lang.label": "Sprache",
      "ui.latest": "Neueste", "hero.noSolder": "Kein Löten",
      "hero.eyebrow": "Quelloffener Schreibtischmonitor",
      "hero.title": "Behalte jeden<br>Bambu&nbsp;Lab-Druck im Blick",
      "hero.lede": "Ein kleines ESP32-S3-Gerät mit 4,3″-Touchscreen, das auf deinem Schreibtisch steht und in Echtzeit zeigt, was deine Drucker gerade tun. Es spricht <strong>ausschließlich</strong> mit deinem selbstgehosteten Bambuddy — deine Drucker sehen also nie eine Cloud von Dritten.",
      "hero.flashBrowser": "Im Browser flashen", "hero.viewGithub": "Auf GitHub ansehen",
      "hero.imgAlt": "Bamboard im 3D-gedruckten Gehäuse von TomE1337, dessen 4,3-Zoll-Bildschirm das Live-Dashboard während eines Drucks zeigt",
      "feat.eyebrow": "Warum Bamboard", "feat.title": "Ein Statusbildschirm auf einen Blick, nach deinen Regeln",
      "feat.intro": "Alles, was Bambuddy bereits über deine Farm weiß — Temperaturen, Fortschritt, AMS, Warteschlange, Verlauf — auf einem eigenen Bildschirm, den du von Anfang bis Ende kontrollierst.",
      "feat.local.h": "Lokal zuerst und privat", "feat.local.p": "Spricht REST mit deiner eigenen Bambuddy-Instanz. Deine Drucker bleiben aus jeder Cloud von Dritten heraus — deine Daten verlassen dein Netzwerk nie, außer du wählst HTTPS.",
      "feat.cheap.h": "Etwa 20 €, kein Löten", "feat.cheap.p": "Eine ESP32-S3-All-in-one-Platine, ein USB-C-Kabel, vier Schrauben und ~70 g Filament fürs Gehäuse. Alles auf einer einzigen Platine — nichts zu verdrahten.",
      "feat.ota.h": "Aktualisiert sich selbst", "feat.ota.p": "Einmal im Browser flashen; danach kommt jede Version per Funk von GitHub. Das Gerät startet jede Nacht neu, sodass neue Firmware unbeaufsichtigt eintrifft.",
      "feat.touch.h": "Touch-orientierte Oberfläche", "feat.touch.p": "Sechs Tabs, tippen oder wischen zum Wechseln. Druck pausieren / fortsetzen / stoppen, das Kammerlicht umschalten oder einen AMS-Trockenzyklus starten — direkt vom Panel.",
      "feat.errors.h": "Lässt keine Fehler durch", "feat.errors.p": "HMS-Fehler erscheinen rot mit einem pulsierenden Vollbildalarm, der sich bis zur Quittierung neu scharf schaltet. Fertige oder fehlgeschlagene Jobs melden sich per Hinweis aus jedem Tab.",
      "feat.langs.h": "Fünf Sprachen", "feat.langs.p": "Wähle die Oberflächensprache beim ersten Start — Englisch, Spanisch, Französisch, Portugiesisch oder Deutsch — ohne erneutes Flashen zum Wechseln.",
      "scr.eyebrow": "Die Bildschirme", "scr.title": "Sechs Tabs, alles auf einen Blick",
      "scr.intro": "Live-Renderings aus einem eingebauten Demo-Datensatz, bei jeder Version neu erzeugt — was du hier siehst, entspricht also immer der aktuellen Firmware.",
      "scr.tab.live.sub": "Temperaturen, Fortschritt & Steuerung", "scr.tab.ams.sub": "Slots, Feuchtigkeit & Trocknung", "scr.tab.printers.sub": "Deine ganze Farm",
      "scr.tab.queue.sub": "Anstehende Jobs der Reihe nach", "scr.tab.history.sub": "Statistiken & jüngste Drucke", "scr.tab.settings.sub": "Diagnose & Helligkeit",
      "scr.alt.live": "Live-Dashboard während eines Drucks — Düsen-, Bett- und Kammertemperaturen, Fortschrittsring, ETA, Kamera-Vorschaubild und Steuerung Pause / Stopp / Geschwindigkeit",
      "scr.alt.ams": "AMS-Übersicht — Filamentfarbe, -typ und Restanteil pro Slot, Feuchtigkeit, Temperatur und ein Live-Trocknungs-Countdown",
      "scr.alt.printers": "Druckerliste — jeder Bambuddy bekannte Drucker, mit Temperaturen und ETA in der Zeile bei den gerade druckenden",
      "scr.alt.queue": "Druckwarteschlange — anstehende Jobs der Reihe nach, jeder mit seinem Zieldrucker",
      "scr.alt.history": "Verlauf und Statistiken — Erfolgsquote, Gesamtfilament, Gesamtzeit und die letzten Drucke",
      "scr.alt.settings": "Einstellungen — Bambuddy-URL und Serverversion, Gerätediagnose, ein Helligkeitsregler und Werksreset",
      "scr.desc.live": "<b>Live-Dashboard.</b> Düsen- / Bett- / Kammertemperaturen, Schichtfortschritt, ETA und Dateiname — plus ein Kamera-Vorschaubild und Pause / Fortsetzen / Stopp in der Zeile, ein Lichtschalter, die Geschwindigkeitsauswahl und die Lüfteranzeige während des Drucks.",
      "scr.desc.ams": "<b>AMS-Übersicht.</b> Filamentfarbe, -typ und Restanteil pro Slot, mit AMS-Feuchtigkeit, -Temperatur und einem Live-Trocknungs-Countdown. Transparentes Filament zeigt ein Schachbrett; eine Trocknen / Stopp-Taste steuert Einheiten mit Heizung auf der eigenen Temperatur des geladenen Filaments.",
      "scr.desc.printers": "<b>Druckerliste.</b> Jeder Bambuddy bekannte Drucker, nach Status hervorgehoben. Druckende Zeilen tragen Temperaturen und ETA in der Zeile; tippe eine an, um sie zu fokussieren und zu Live zu springen.",
      "scr.desc.queue": "<b>Druckwarteschlange.</b> Die Jobs, die Bambuddy noch anstehen hat, der Reihe nach, jeder mit seinem Zieldrucker gekennzeichnet.",
      "scr.desc.history": "<b>Verlauf & Statistiken.</b> Die letzten Drucke neben Gesamtkennzahlen — Erfolgsquote, Gesamtfilament und Gesamtdruckzeit.",
      "scr.desc.settings": "<b>Einstellungen.</b> Bambuddy-URL und Version + Laufzeit des Servers, lokale IP, WLAN-RSSI und Gerätelaufzeit, ein Helligkeitsregler 1–5, eine Wi-Fi-Taste (öffnet das Captive-Portal erneut ohne Löschen) und ein Werksreset mit zwei Tipps.",
      "how.eyebrow": "So funktioniert's", "how.title": "Deine Drucker → Bambuddy → Bamboard",
      "how.intro": "Bamboard ist ein schlanker, dedizierter Client für die Bambuddy-API. Keine Brücke, kein zusätzlicher Dienst außer dem Bambuddy, das du bereits hostest.",
      "how.node.printers": "Drucker", "how.node.selfhosted": "selbstgehostet", "how.node.device": "das Gerät",
      "how.note.h": "HTTP im LAN oder HTTPS zu einer Domain", "how.note.p": "validiert gegen ein per CI aktualisiertes CA-Bündel · optionaler Cloudflare-Access-Token",
      "how.rt.h": "Live-Status.", "how.rt.p": "Fragt Bambuddys REST-API alle 2 s je Drucker ab (<code>printer_status</code>): Temperaturen, Fortschritt und HMS-Alarme erscheinen in wenigen Sekunden — nur API-Schlüssel-Auth, nichts zum Einloggen.",
      "how.lan.h": "LAN oder per Fern, deine Wahl.", "how.lan.p": "Standardmäßig einfaches HTTP zu einer privaten IP / <code>.local</code> in deinem LAN, oder HTTPS zu einer Domain mit dem Serverzertifikat, validiert gegen ein eingebautes, per CI aktualisiertes Stamm-CA-Bündel.",
      "how.cf.h": "Bereit für Cloudflare Access.", "how.cf.p": "Optionale Service-Token-Felder lassen dich ein hinter Cloudflare Access veröffentlichtes Bambuddy erreichen — die Header reisen nur über HTTPS / WSS, nie im Klartext.",
      "how.setup.h": "Einrichtung ohne erneutes Flashen.", "how.setup.p": "Ein Captive-Portal beim ersten Start erfasst WLAN, Bambuddy-URL + API-Schlüssel, Zeitzone, tägliche Neustartstunde und Sprache. Halte beim Einschalten <kbd>BOOT</kbd>, um es erneut auszuführen.",
      "hw.eyebrow": "Hardware", "hw.title": "Eine Platine. Keine Verdrahtung. Kein Löten.",
      "hw.intro": "Keine DevKit-Verdrahtung, keine Breakout-Boards. Eine einzige Guition-All-in-one passt in ein druckbares Gehäuse.",
      "hw.spec.board": "<b>Guition JC4827W543</b> — ESP32-S3-WROOM-1 mit 8&nbsp;MB PSRAM, alles auf einer Platine.",
      "hw.spec.panel": "<b>4,3″-IPS, 480 × 272</b> RGB-Parallel-Panel mit kapazitiver GT911-Touch-Schicht.",
      "hw.spec.case": "<b>FDM-druckbares Gehäuse</b> — ein dreiteiliges Community-Design von <a href=\"https://www.printables.com/@TomE1337_2178164\" target=\"_blank\" rel=\"noopener\">TomE1337</a> (CC BY-NC-SA): Korpus + Deckel + Schreibtischständer. ~120&nbsp;g Filament.",
      "hw.spec.usb": "<b>Nur ein USB-C-Kabel</b> für Strom und das einmalige Flashen — sonst nichts anzuschließen.",
      "hw.bomBtn": "Stückliste", "hw.bom.total": "Stückliste gesamt",
      "hw.bom.board": "All-in-one-Platine", "hw.bom.usb": "USB-C-Datenkabel", "hw.bom.fasteners": "Gehäuseschrauben", "hw.bom.fastenersV": "je nach Modell", "hw.bom.fil": "Filament (Gehäuse)", "hw.bom.solder": "Löten", "hw.bom.solderV": "keines",
      "inst.eyebrow": "Installation & Updates", "inst.title": "Flashe es direkt von dieser Seite",
      "inst.intro": "Die erste Installation läuft im Browser über Web Serial — kein PlatformIO, keine CLI, keine Toolchain. Alles danach kommt per Funk.",
      "inst.browser.h": "Browser-Installation", "inst.browser.p": "Chrome, Edge oder Opera auf einem Computer. Stecke die Platine an ein USB-C-Datenkabel und klicke auf Verbinden.",
      "inst.unsupported": "⚠️ Dieser Browser kann nicht mit USB-Seriell-Geräten kommunizieren. Öffne diese Seite in <strong>Chrome, Edge oder Opera auf einem Computer</strong>, um zu flashen.",
      "inst.notallowed": "⚠️ Web Serial benötigt eine sichere (https) Seite.",
      "inst.placeholder": "⚠️ Das veröffentlichte Manifest ist ein Platzhalter (<code>0.0.0-dev</code>) — die Release-CI ist noch nicht gelaufen. Nutze stattdessen die <a href=\"https://github.com/clabeuhtegrite/Bamboard/releases/latest\" target=\"_blank\" rel=\"noopener\">neueste Version</a> auf GitHub.",
      "inst.step1": "Nutze <b>Chrome, Edge oder Opera</b> auf einem Computer (Web Serial gibt es nicht in Firefox, Safari oder auf Telefonen).",
      "inst.step2": "Stecke die Platine an ein USB-C-<b>Datenkabel</b> (nicht nur Laden).",
      "inst.step3": "Klicke auf <b>Verbinden</b> und wähle den erscheinenden seriellen Port.",
      "inst.step4": "Kein Port? Geh in den Download-Modus: <kbd>BOOT</kbd> halten, <kbd>RST</kbd> tippen, <kbd>BOOT</kbd> loslassen, erneut versuchen.",
      "inst.step5": "Beim ersten Start dem WLAN <code>Bamboard-setup</code> beitreten und die Einrichtung im Captive-Portal abschließen.",
      "inst.note": "Das machst du nur einmal. Nach dem ersten Flashen aktualisiert sich das Gerät bei jedem Start per Funk von GitHub Releases.",
      "inst.other.h": "Andere Browser & manuelles Flashen",
      "inst.other.p": "Web Serial gibt es nur für Chromium am Desktop. Unter Firefox, Safari, Linux ohne Chromium-Browser, oder zum manuellen Flashen, lade <code>bamboard-factory.bin</code> aus der <a href=\"https://github.com/clabeuhtegrite/Bamboard/releases/latest\" target=\"_blank\" rel=\"noopener\">neuesten Version</a> und schreibe es an Offset <code>0x0</code> mit <a href=\"https://github.com/espressif/esptool\" target=\"_blank\" rel=\"noopener\">esptool</a>:",
      "inst.copy": "Befehl kopieren",
      "cta.eyebrow": "Loslegen", "cta.title": "Bau dein eigenes Bamboard",
      "cta.p": "Bestelle die ~20-€-Platine, drucke das Gehäuse und flashe es im Browser. Die Doku führt dich durch Zusammenbau, Flashen und Konfiguration.",
      "cta.flashNow": "Jetzt flashen", "cta.readDocs": "Doku lesen",
      "foot.tagline": "Ein quelloffener Touch-Schreibtischmonitor für Bambu-Lab-Drucker, angetrieben von selbstgehostetem Bambuddy.",
      "foot.h.project": "Projekt", "foot.h.docs": "Doku", "foot.h.related": "Verwandtes",
      "foot.repo": "GitHub-Repository", "foot.releases": "Releases", "foot.changelog": "Änderungsprotokoll", "foot.license": "MIT-Lizenz",
      "foot.assembly": "Zusammenbau", "foot.flashing": "Flashen", "foot.config": "Konfiguration", "foot.bom": "Stückliste",
      "foot.bottom1": "MIT-lizenziert · Nicht mit Bambu Lab oder dem Bambuddy-Projekt verbunden.", "foot.bottom2": "Gebaut für Maker, die selbst hosten.",
      "lb.label": "Vergrößerter Screenshot", "lb.close": "Schließen"
    }
  }/*i18n:json*/;

  // ---- helpers -----------------------------------------------------------
  var qsa = function (s) { return Array.prototype.slice.call(document.querySelectorAll(s)); };
  function supported(code) { return LANGS.indexOf(code) >= 0; }

  function detect() {
    var sp;
    try { sp = new URLSearchParams(location.search).get("lang"); } catch (e) {}
    if (sp) { sp = sp.toLowerCase().split("-")[0]; if (supported(sp)) { save(sp); return sp; } }
    var saved = null;
    try { saved = localStorage.getItem("bb-lang"); } catch (e) {}
    if (saved && supported(saved)) return saved;
    var cand = (navigator.languages && navigator.languages.length) ? navigator.languages
                                                                    : [navigator.language || "en"];
    for (var i = 0; i < cand.length; i++) {
      var p = (cand[i] || "").toLowerCase().split("-")[0];
      if (supported(p)) return p;
    }
    return "en";
  }
  function save(code) { try { localStorage.setItem("bb-lang", code); } catch (e) {} }

  var current = "en";
  var enTitle = document.title;
  var metaEl = document.querySelector('meta[name="description"]');
  var enDesc = metaEl ? metaEl.getAttribute("content") : "";

  function apply(lang) {
    current = supported(lang) ? lang : "en";
    var d = DICTS[current] || {};
    document.documentElement.setAttribute("lang", current);

    qsa("[data-i18n]").forEach(function (el) {
      if (el.__en == null) el.__en = el.textContent;
      var v = d[el.getAttribute("data-i18n")];
      el.textContent = (typeof v === "string") ? v : el.__en;
    });
    qsa("[data-i18n-html]").forEach(function (el) {
      if (el.__enh == null) el.__enh = el.innerHTML;
      var v = d[el.getAttribute("data-i18n-html")];
      el.innerHTML = (typeof v === "string") ? v : el.__enh;
    });
    qsa("[data-i18n-attr]").forEach(function (el) {
      if (!el.__attr) el.__attr = {};
      el.getAttribute("data-i18n-attr").split(";").forEach(function (pair) {
        var i = pair.indexOf(":"); if (i < 0) return;
        var a = pair.slice(0, i).trim(), k = pair.slice(i + 1).trim();
        if (el.__attr[a] == null) el.__attr[a] = el.getAttribute(a) || "";
        var v = d[k];
        el.setAttribute(a, (typeof v === "string") ? v : el.__attr[a]);
      });
    });

    document.title = (typeof d["meta.title"] === "string") ? d["meta.title"] : enTitle;
    if (metaEl) metaEl.setAttribute("content", (typeof d["meta.desc"] === "string") ? d["meta.desc"] : enDesc);

    var sel = document.getElementById("langSelect");
    if (sel && sel.value !== current) sel.value = current;

    // Let app.js re-render JS-managed text (the showcase descriptions).
    try { document.dispatchEvent(new CustomEvent("bb:lang", { detail: { lang: current } })); } catch (e) {}
  }

  // Exposed for app.js (showcase descriptions live in this dictionary too).
  window.BB_I18N = {
    get lang() { return current; },
    t: function (key) { var d = DICTS[current]; return d ? d[key] : undefined; }
  };

  function buildSwitcher() {
    var host = document.getElementById("langSwitch");
    if (!host) return;
    var sel = document.createElement("select");
    sel.id = "langSelect";
    sel.className = "lang-select";
    var dd = DICTS[current] || DICTS[detect()] || {};
    sel.setAttribute("aria-label", dd["lang.label"] || "Language");
    LANGS.forEach(function (code) {
      var o = document.createElement("option");
      o.value = code; o.textContent = NAMES[code];
      if (code === current) o.selected = true;
      sel.appendChild(o);
    });
    sel.addEventListener("change", function () { save(sel.value); apply(sel.value); });
    host.appendChild(sel);
  }

  function init() {
    current = detect();
    buildSwitcher();
    apply(current);
  }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
  else init();
})();
