/* Bamboard showcase — progressive enhancement. The page is fully readable
   without JS; this adds the version badge, the interactive screen showcase,
   the lightbox, the copy button, mobile nav and scroll reveals. */
(function () {
  "use strict";
  var $ = function (s, r) { return (r || document).querySelector(s); };
  var $$ = function (s, r) { return Array.prototype.slice.call((r || document).querySelectorAll(s)); };

  /* ---- Firmware version from the ESP Web Tools manifest ----------------- */
  // Always show what the page will install, without hard-coding it. When the
  // version is still the in-tree placeholder ("0.0.0-dev"), hide the install
  // button — the release CI hasn't regenerated the manifest yet and Connecting
  // would flash a dev sentinel build nobody wants.
  fetch("./manifest.json").then(function (r) { return r.json(); }).then(function (m) {
    var ver = (m && m.version) || "";
    $$("#ver, #ver2").forEach(function (el) { el.textContent = ver || "?"; });
    if (!ver || ver === "0.0.0-dev") {
      var btn = $("#installBtn"); if (btn) btn.hidden = true;
      var warn = $("#placeholderWarn"); if (warn) warn.hidden = false;
    }
  }).catch(function () {
    $$("#ver, #ver2").forEach(function (el) { el.textContent = "?"; });
  });

  /* ---- Sticky nav state + mobile menu ---------------------------------- */
  var nav = $("#nav");
  var onScroll = function () { if (nav) nav.classList.toggle("scrolled", window.scrollY > 8); };
  onScroll();
  window.addEventListener("scroll", onScroll, { passive: true });

  var toggle = $("#navToggle");
  if (toggle && nav) {
    toggle.addEventListener("click", function () {
      var open = nav.classList.toggle("open");
      toggle.setAttribute("aria-expanded", open ? "true" : "false");
    });
    $$(".nav-links a").forEach(function (a) {
      a.addEventListener("click", function () {
        nav.classList.remove("open");
        toggle.setAttribute("aria-expanded", "false");
      });
    });
  }

  /* ---- Screen showcase ------------------------------------------------- */
  var SCREENS = {
    live:     ["01", "<b>Live dashboard.</b> Nozzle / bed / chamber temps, layer progress, ETA and filename — plus a camera thumbnail and inline Pause / Resume / Stop, a chamber-light toggle, the print-speed picker and a fan readout while printing."],
    ams:      ["02", "<b>AMS overview.</b> Per-slot filament colour, type and remaining percent, with AMS humidity, temperature and a live drying countdown. Clear filament shows a checkerboard; a Dry / Stop pill drives heater-equipped units at the loaded filament’s own temperature."],
    printers: ["03", "<b>Printers list.</b> Every printer Bambuddy knows, highlighted by state. Rows that are printing carry inline temperatures and ETA; tap one to focus it and jump to Live."],
    queue:    ["04", "<b>Print queue.</b> The jobs Bambuddy still has pending, in order, each tagged with its target printer."],
    history:  ["05", "<b>History &amp; stats.</b> The last prints alongside lifetime KPIs — success rate, total filament and total print time."],
    settings: ["06", "<b>Settings.</b> Bambuddy URL and the server’s version + uptime, local IP, Wi-Fi RSSI and device uptime, a 1–5 brightness selector, and a two-tap factory reset."]
  };
  var showcase = $("#showcase");
  var tabs = $$(".screen-tab");
  if (showcase && tabs.length) {
    showcase.classList.add("js"); // enables the one-at-a-time stage (CSS)
    var shots = $$(".shot", showcase);
    var numEl = $("#shotNum"), descEl = $("#shotDesc");

    var activeKey = "live";
    // Showcase descriptions live in the i18n dictionary (scr.desc.*) so they
    // follow the active language; fall back to the English SCREENS table.
    var descFor = function (key) {
      var v = (window.BB_I18N && window.BB_I18N.t) ? window.BB_I18N.t("scr.desc." + key) : null;
      return (typeof v === "string" && v) ? v : (SCREENS[key] ? SCREENS[key][1] : "");
    };

    var select = function (key, focus) {
      activeKey = key;
      tabs.forEach(function (t) {
        var on = t.getAttribute("data-screen") === key;
        t.classList.toggle("is-active", on);
        t.setAttribute("aria-selected", on ? "true" : "false");
        t.tabIndex = on ? 0 : -1;
        if (on && focus) t.focus();
      });
      shots.forEach(function (s) { s.classList.toggle("is-active", s.getAttribute("data-screen") === key); });
      if (SCREENS[key]) {
        if (numEl) numEl.textContent = SCREENS[key][0];
        if (descEl) descEl.innerHTML = descFor(key);
      }
    };

    tabs.forEach(function (t, i) {
      t.tabIndex = t.classList.contains("is-active") ? 0 : -1;
      t.addEventListener("click", function () { select(t.getAttribute("data-screen")); });
      t.addEventListener("keydown", function (e) {
        if (e.key !== "ArrowRight" && e.key !== "ArrowLeft") return;
        e.preventDefault();
        var n = (i + (e.key === "ArrowRight" ? 1 : tabs.length - 1)) % tabs.length;
        select(tabs[n].getAttribute("data-screen"), true);
      });
    });

    // Render the initial description in the detected language, and re-render it
    // when the visitor switches language (i18n.js dispatches bb:lang).
    select(activeKey);
    document.addEventListener("bb:lang", function () {
      if (descEl) descEl.innerHTML = descFor(activeKey);
    });
  }

  /* ---- Lightbox -------------------------------------------------------- */
  var lb = $("#lightbox"), lbImg = $("#lightboxImg"), lbClose = $("#lightboxClose");
  if (lb && lbImg) {
    var openLb = function (src, alt) {
      lbImg.src = src; lbImg.alt = alt || "";
      lb.classList.add("open"); document.body.style.overflow = "hidden";
    };
    var closeLb = function () { lb.classList.remove("open"); lbImg.src = ""; document.body.style.overflow = ""; };
    $$(".device-screen .shot").forEach(function (img) {
      img.addEventListener("click", function () { openLb(img.currentSrc || img.src, img.alt); });
    });
    if (lbClose) lbClose.addEventListener("click", closeLb);
    lb.addEventListener("click", function (e) { if (e.target === lb) closeLb(); });
    document.addEventListener("keydown", function (e) { if (e.key === "Escape" && lb.classList.contains("open")) closeLb(); });
  }

  /* ---- Copy esptool command ------------------------------------------- */
  var copyBtn = $("#copyBtn"), cmd = $("#esptoolCmd");
  if (copyBtn && cmd && navigator.clipboard) {
    copyBtn.addEventListener("click", function () {
      navigator.clipboard.writeText(cmd.textContent.trim()).then(function () {
        copyBtn.classList.add("copied");
        copyBtn.innerHTML = '<svg aria-hidden="true"><use href="#i-check"/></svg>';
        setTimeout(function () {
          copyBtn.classList.remove("copied");
          copyBtn.innerHTML = '<svg aria-hidden="true"><use href="#i-copy"/></svg>';
        }, 1600);
      }).catch(function () {});
    });
  }

  /* ---- Scroll reveal --------------------------------------------------- */
  var reveals = $$(".reveal");
  if ("IntersectionObserver" in window && reveals.length) {
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (en) {
        if (en.isIntersecting) { en.target.classList.add("in"); io.unobserve(en.target); }
      });
    }, { rootMargin: "0px 0px -8% 0px", threshold: 0.06 });
    reveals.forEach(function (el) { io.observe(el); });
  } else {
    reveals.forEach(function (el) { el.classList.add("in"); });
  }
})();
