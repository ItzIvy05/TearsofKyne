(() => {
  const accessibilityThemes = [
    {
      menuAccent: '#ff7139',
      menuAccent2: '#ff8a00',
      detailedStageColors: ['#ff9a64', '#ff8a48', '#ff7139', '#ff5d22', '#e0461a'],
      simpleStageColors: ['#31d16f', '#6bcf54', '#b9c63a', '#ee9733', '#df4a4a'],
    },
    {
      menuAccent: '#4dc4ff',
      menuAccent2: '#82ddff',
      detailedStageColors: ['#b8f0ff', '#82ddff', '#4dc4ff', '#2499ea', '#156fcb'],
      simpleStageColors: ['#b8f0ff', '#82ddff', '#4dc4ff', '#2499ea', '#156fcb'],
    },
    {
      menuAccent: '#26bfb6',
      menuAccent2: '#57dfce',
      detailedStageColors: ['#c8fff4', '#94f3e3', '#57dfce', '#26bfb6', '#158f99'],
      simpleStageColors: ['#c8fff4', '#94f3e3', '#57dfce', '#26bfb6', '#158f99'],
    },
    {
      menuAccent: '#f760b3',
      menuAccent2: '#ff89cb',
      detailedStageColors: ['#ffd5ef', '#ffb1de', '#ff89cb', '#f760b3', '#d63e93'],
      simpleStageColors: ['#ffd5ef', '#ffb1de', '#ff89cb', '#f760b3', '#d63e93'],
    },
    {
      menuAccent: '#ffc94e',
      menuAccent2: '#ffe074',
      detailedStageColors: ['#fff1a8', '#ffe074', '#ffc94e', '#f2ad21', '#d98d00'],
      simpleStageColors: ['#fff1a8', '#ffe074', '#ffc94e', '#f2ad21', '#d98d00'],
    },
    {
      menuAccent: '#9d73f3',
      menuAccent2: '#bb98ff',
      detailedStageColors: ['#ebe1ff', '#d5c2ff', '#bb98ff', '#9d73f3', '#7d51d8'],
      simpleStageColors: ['#ebe1ff', '#d5c2ff', '#bb98ff', '#9d73f3', '#7d51d8'],
    },
  ];

  const root = document.getElementById('hud-root');
  const panel = document.getElementById('water-panel');
  const simpleWidget = document.getElementById('simple-widget');
  const simpleDropShell = document.getElementById('simple-drop-shell');
  const simpleDropShape = document.getElementById('simple-drop-shape');
  const accent = document.getElementById('water-accent');
  const drop = document.querySelector('.water-drop');
  const dropShape = document.getElementById('water-drop-shape');
  const stage = document.getElementById('water-stage');
  const difficulty = document.getElementById('water-difficulty');
  const progress = document.getElementById('water-progress-fill');
  const percent = document.getElementById('water-percent');
  const notification = document.getElementById('water-notification');
  const hotkeys = document.getElementById('water-hotkeys');
  const fillHotkey = document.getElementById('fill-hotkey');
  const fillHotkeyKey = document.getElementById('fill-hotkey-key');

  const overlay = document.getElementById('settings-overlay');
  const settingsWindow = document.getElementById('settings-window');
  const settingsDragHandle = document.getElementById('settings-drag-handle');
  const closeButton = document.getElementById('settings-close');
  const bindFillButton = document.getElementById('bind-fill');
  const bindMenuButton = document.getElementById('bind-menu');
  const bindToggleHudButton = document.getElementById('bind-toggle-hud');
  const widgetStyleSelect = document.getElementById('widget-style');
  const difficultySelect = document.getElementById('difficulty-select');
  const accessibilitySelect = document.getElementById('accessibility-select');
  const hudXValue = document.getElementById('hud-x');
  const hudYValue = document.getElementById('hud-y');
  const hudXSlider = document.getElementById('hud-x-slider');
  const hudYSlider = document.getElementById('hud-y-slider');
  const widgetScaleValue = document.getElementById('widget-scale-value');
  const widgetScaleSlider = document.getElementById('widget-scale-slider');
  const resetPositionButton = document.getElementById('reset-position');
  const toggleWaterSystemButton = document.getElementById('toggle-water-system');
  const disableConfirmOverlay = document.getElementById('disable-confirm-overlay');
  const disableConfirmCancel = document.getElementById('disable-confirm-cancel');
  const disableConfirmAccept = document.getElementById('disable-confirm-accept');

  let notificationTimer = 0;
  let settingsOpen = false;
  let awaitingBinding = '';
  let hudX = 24;
  let hudY = 28;
  let hudVisible = true;
  let widgetStyle = 0;
  let widgetMenuSuppressed = true;
  let waterSystemEnabled = true;
  let menuX = -1;
  let menuY = -1;
  let widgetScale = 100;
  let accessibilityTheme = 0;
  let currentStage = 0;
  let hudDragState = null;
  let menuDragState = null;

  const bindButtons = {
    fill: bindFillButton,
    menu: bindMenuButton,
    togglehud: bindToggleHudButton,
  };

  const getActiveWidget = () => (widgetStyle === 1 ? simpleWidget : panel);
  const getActiveWidgetRect = () => getActiveWidget().getBoundingClientRect();
  const clampValue = (value, min, max) => Math.max(min, Math.min(max, value));

  const hexToRgb = (hex) => {
    const normalized = String(hex || '').replace('#', '').trim();
    if (normalized.length !== 6) {
      return { r: 255, g: 113, b: 57 };
    }
    return {
      r: Number.parseInt(normalized.slice(0, 2), 16),
      g: Number.parseInt(normalized.slice(2, 4), 16),
      b: Number.parseInt(normalized.slice(4, 6), 16),
    };
  };

  const hexToRgba = (hex, alpha) => {
    const { r, g, b } = hexToRgb(hex);
    return `rgba(${r}, ${g}, ${b}, ${alpha})`;
  };

  const mixHex = (baseHex, targetHex, baseWeight = 0.5) => {
    const a = hexToRgb(baseHex);
    const b = hexToRgb(targetHex);
    const weight = clampValue(Number(baseWeight) || 0, 0, 1);
    const inv = 1 - weight;
    return {
      r: Math.round((a.r * weight) + (b.r * inv)),
      g: Math.round((a.g * weight) + (b.g * inv)),
      b: Math.round((a.b * weight) + (b.b * inv)),
    };
  };

  const rgbToRgba = (rgb, alpha) => `rgba(${rgb.r}, ${rgb.g}, ${rgb.b}, ${alpha})`;
  const mixRgb = (a, b, weight = 0.5) => ({
    r: Math.round((a.r * weight) + (b.r * (1 - weight))),
    g: Math.round((a.g * weight) + (b.g * (1 - weight))),
    b: Math.round((a.b * weight) + (b.b * (1 - weight))),
  });
  const rgbToCss = (rgb) => `rgb(${rgb.r}, ${rgb.g}, ${rgb.b})`;
  const srgbChannelToLinear = (value) => {
    const channel = value / 255;
    return channel <= 0.04045 ? channel / 12.92 : Math.pow((channel + 0.055) / 1.055, 2.4);
  };
  const getRelativeLuminance = (rgb) => (0.2126 * srgbChannelToLinear(rgb.r)) + (0.7152 * srgbChannelToLinear(rgb.g)) + (0.0722 * srgbChannelToLinear(rgb.b));
  const getContrastRatio = (a, b) => {
    const l1 = getRelativeLuminance(a);
    const l2 = getRelativeLuminance(b);
    const lighter = Math.max(l1, l2);
    const darker = Math.min(l1, l2);
    return (lighter + 0.05) / (darker + 0.05);
  };
  const ensureContrast = (fg, bg, minRatio) => {
    let candidate = { ...fg };
    const white = { r: 255, g: 255, b: 255 };
    for (let i = 0; i < 16 && getContrastRatio(candidate, bg) < minRatio; i += 1) {
      candidate = mixRgb(candidate, white, 0.68);
    }
    return candidate;
  };

  const getTheme = () => accessibilityThemes[clampValue(accessibilityTheme, 0, accessibilityThemes.length - 1)] || accessibilityThemes[0];

  const applyAccessibilityTheme = () => {
    const theme = getTheme();
    const rootStyle = document.documentElement.style;
    const shellTop = mixHex(theme.menuAccent, '#101722', 0.18);
    const shellBottom = mixHex(theme.menuAccent2, '#090d14', 0.10);
    const panelTop = mixHex(theme.menuAccent, '#182230', 0.15);
    const panelBottom = mixHex(theme.menuAccent2, '#0d141d', 0.10);
    const buttonTop = mixHex(theme.menuAccent, '#f4f7fb', 0.08);
    const buttonBottom = mixHex(theme.menuAccent2, '#0f1722', 0.14);
    const isDefaultTheme = accessibilityTheme === 0;
    const textBase = isDefaultTheme
      ? { r: 255, g: 255, b: 255 }
      : ensureContrast(mixHex(theme.menuAccent, '#ffffff', 0.16), panelBottom, 10.0);
    const textSoft = isDefaultTheme
      ? { r: 255, g: 255, b: 255 }
      : ensureContrast(mixHex(theme.menuAccent2, '#edf4ff', 0.24), panelBottom, 7.2);
    const textDim = isDefaultTheme
      ? { r: 255, g: 255, b: 255 }
      : ensureContrast(mixHex(theme.menuAccent2, '#d5dfec', 0.28), panelBottom, 4.8);
    const accentText = isDefaultTheme
      ? { r: 255, g: 255, b: 255 }
      : ensureContrast(mixHex(theme.menuAccent, '#ffffff', 0.48), panelTop, 6.4);
    const accentStrongText = isDefaultTheme
      ? { r: 255, g: 255, b: 255 }
      : ensureContrast(mixHex(theme.menuAccent2, '#ffffff', 0.56), panelTop, 7.4);

    rootStyle.setProperty('--theme-accent', theme.menuAccent);
    rootStyle.setProperty('--theme-accent-2', theme.menuAccent2);
    rootStyle.setProperty('--theme-accent-soft', hexToRgba(theme.menuAccent, 0.22));
    rootStyle.setProperty('--theme-accent-soft-2', hexToRgba(theme.menuAccent2, 0.18));
    rootStyle.setProperty('--theme-accent-border', hexToRgba(theme.menuAccent, 0.30));
    rootStyle.setProperty('--theme-accent-border-strong', hexToRgba(theme.menuAccent, 0.48));
    rootStyle.setProperty('--theme-accent-faint', hexToRgba(theme.menuAccent, 0.10));
    rootStyle.setProperty('--theme-accent-ring', hexToRgba(theme.menuAccent, 0.14));
    rootStyle.setProperty('--theme-accent-glow', hexToRgba(theme.menuAccent, 0.32));
    rootStyle.setProperty('--theme-accent-strong', hexToRgba(theme.menuAccent, 0.84));
    rootStyle.setProperty('--theme-accent-strong-2', hexToRgba(theme.menuAccent2, 0.90));
    rootStyle.setProperty('--text-0', rgbToCss(textBase));
    rootStyle.setProperty('--text-1', rgbToRgba(textSoft, 0.82));
    rootStyle.setProperty('--text-2', rgbToRgba(textDim, 0.60));
    rootStyle.setProperty('--text-accent', rgbToCss(accentText));
    rootStyle.setProperty('--text-accent-strong', rgbToCss(accentStrongText));
    rootStyle.setProperty('--text-chevron', rgbToRgba(textSoft, 0.84));

    rootStyle.setProperty('--bg-0', rgbToRgba(panelBottom, 0.84));
    rootStyle.setProperty('--bg-1', rgbToRgba(panelTop, 0.95));
    rootStyle.setProperty('--bg-2', rgbToRgba(mixHex(theme.menuAccent, '#233243', 0.18), 0.96));
    rootStyle.setProperty('--shell-top', rgbToRgba(shellTop, 0.96));
    rootStyle.setProperty('--shell-bottom', rgbToRgba(shellBottom, 0.94));
    rootStyle.setProperty('--panel-top', rgbToRgba(panelTop, 0.95));
    rootStyle.setProperty('--panel-bottom', rgbToRgba(panelBottom, 0.92));
    rootStyle.setProperty('--button-top', rgbToRgba(buttonTop, 0.16));
    rootStyle.setProperty('--button-bottom', rgbToRgba(buttonBottom, 0.66));
    rootStyle.setProperty('--surface-1', rgbToRgba(mixHex(theme.menuAccent, '#f8fbff', 0.05), 0.10));
    rootStyle.setProperty('--surface-2', rgbToRgba(mixHex(theme.menuAccent2, '#0a0f15', 0.12), 0.12));
    rootStyle.setProperty('--surface-3', rgbToRgba(mixHex(theme.menuAccent, '#eff4fb', 0.04), 0.08));
    rootStyle.setProperty('--surface-4', rgbToRgba(mixHex(theme.menuAccent2, '#0c1118', 0.08), 0.06));
  };

  const applyStageColors = () => {
    const theme = getTheme();
    const safeStage = clampValue(currentStage, 0, 4);
    const detailedColor = theme.detailedStageColors[safeStage];
    const simpleColor = theme.simpleStageColors[safeStage];
    setDetailedColor(detailedColor, hexToRgba(detailedColor, 0.32));
    setSimpleColor(simpleColor, hexToRgba(simpleColor, 0.30));
  };

  const updateRangeVisual = (slider) => {
    const min = Number(slider.min || '0');
    const max = Number(slider.max || '100');
    const value = Number(slider.value || '0');
    const ratio = max > min ? ((value - min) / (max - min)) * 100 : 0;
    slider.style.setProperty('--slider-fill', `${Math.max(0, Math.min(100, ratio))}%`);
  };

  const updateNotificationAnchor = (forceMeasure = false) => {
    const rect = getActiveWidgetRect();
    const safeWidth = Math.max(52, Math.round(rect.width || 0));
    const safeHeight = Math.max(52, Math.round(rect.height || 0));
    root.style.width = `${safeWidth}px`;
    root.style.height = `${safeHeight}px`;
    root.style.setProperty('--notification-width', `${Math.max(176, safeWidth)}px`);

    const hadHiddenClass = notification.classList.contains('hidden');
    if (forceMeasure && hadHiddenClass) {
      notification.classList.remove('hidden');
      notification.style.visibility = 'hidden';
    }

    const notificationHeight = Math.max(44, Math.round(notification.getBoundingClientRect().height || 0));
    const topSpace = Math.max(0, rect.top);
    const bottomSpace = Math.max(0, window.innerHeight - rect.bottom);
    const shouldShowBelow = topSpace < notificationHeight + 18 && bottomSpace > topSpace;
    notification.classList.toggle('notification-below', shouldShowBelow);

    if (forceMeasure && hadHiddenClass) {
      notification.classList.add('hidden');
      notification.style.visibility = '';
    }
  };

  const refreshSliderBounds = () => {
    const rect = getActiveWidgetRect();
    hudXSlider.max = String(Math.max(0, Math.round(window.innerWidth - rect.width - 8)));
    hudYSlider.max = String(Math.max(0, Math.round(window.innerHeight - rect.height - 8)));
    updateRangeVisual(hudXSlider);
    updateRangeVisual(hudYSlider);
    updateNotificationAnchor();
  };

  const updateWaterSystemButton = () => {
    toggleWaterSystemButton.classList.toggle('is-on', waterSystemEnabled);
    toggleWaterSystemButton.classList.toggle('is-off', !waterSystemEnabled);
    toggleWaterSystemButton.setAttribute('aria-checked', waterSystemEnabled ? 'true' : 'false');
  };

  const closeDisableConfirm = () => {
    disableConfirmOverlay.classList.add('hidden');
  };

  const applyWidgetScale = () => {
    const clamped = clampValue(Math.round(widgetScale), 50, 150);
    widgetScale = clamped;
    document.documentElement.style.setProperty('--widget-scale', String(clamped / 100));
    widgetScaleValue.textContent = `${clamped}%`;
    widgetScaleSlider.value = String(clamped);
    updateRangeVisual(widgetScaleSlider);
    refreshSliderBounds();
    updateHudPosition(hudX, hudY, false);
  };

  const setDetailedColor = (color, glow) => {
    accent.style.background = color;
    stage.style.color = color;
    stage.style.textShadow = `0 0 10px ${glow}`;
    progress.style.background = color;
    progress.style.boxShadow = `0 0 8px ${glow}`;
    panel.style.boxShadow = `0 16px 40px rgba(0, 0, 0, 0.42), 0 0 28px ${glow}`;
    notification.style.borderLeftColor = color;
    if (drop) {
      drop.style.filter = `drop-shadow(0 0 5px ${glow})`;
    }
    if (dropShape) {
      dropShape.setAttribute('fill', color);
    }
  };

  const setSimpleColor = (color, glow) => {
    simpleDropShape.setAttribute('fill', color);
    simpleDropShell.style.filter = `drop-shadow(0 0 12px ${glow})`;
  };

  const applyWidgetStyle = () => {
    const hideDetailed = !waterSystemEnabled || widgetStyle === 1 || widgetMenuSuppressed || (!hudVisible && !settingsOpen);
    const hideSimple = !waterSystemEnabled || widgetStyle !== 1 || widgetMenuSuppressed || (!hudVisible && !settingsOpen);
    panel.classList.toggle('nordic-style', widgetStyle === 2);
    notification.classList.toggle('nordic-style', widgetStyle === 2);
    panel.classList.toggle('hidden', hideDetailed);
    simpleWidget.classList.toggle('hidden', hideSimple);
    notification.classList.toggle('hidden', !waterSystemEnabled || widgetMenuSuppressed || !notification.textContent);
    refreshSliderBounds();
    updateHudPosition(hudX, hudY, false);
  };

  const updateHudPosition = (x, y, save) => {
    refreshSliderBounds();
    const maxX = Number(hudXSlider.max || '0');
    const maxY = Number(hudYSlider.max || '0');
    hudX = Math.max(0, Math.min(maxX, Math.round(x)));
    hudY = Math.max(0, Math.min(maxY, Math.round(y)));
    root.style.left = `${hudX}px`;
    root.style.bottom = `${hudY}px`;
    hudXValue.textContent = String(hudX);
    hudYValue.textContent = String(hudY);
    hudXSlider.value = String(hudX);
    hudYSlider.value = String(hudY);
    updateRangeVisual(hudXSlider);
    updateRangeVisual(hudYSlider);

    if (save && typeof window.tokSaveHudPosition === 'function') {
      window.tokSaveHudPosition(`${hudX}|${hudY}`);
    }
  };

  const clampMenuPosition = (x, y) => {
    const maxX = Math.max(0, window.innerWidth - settingsWindow.offsetWidth - 16);
    const maxY = Math.max(0, window.innerHeight - settingsWindow.offsetHeight - 16);
    return {
      x: Math.max(16, Math.min(maxX, Math.round(x))),
      y: Math.max(16, Math.min(maxY, Math.round(y))),
    };
  };

  const centerMenu = () => {
    const centeredX = Math.max(16, Math.round((window.innerWidth - settingsWindow.offsetWidth) / 2));
    const centeredY = Math.max(16, Math.round((window.innerHeight - settingsWindow.offsetHeight) / 2));
    menuX = centeredX;
    menuY = centeredY;
    settingsWindow.style.left = `${menuX}px`;
    settingsWindow.style.top = `${menuY}px`;
  };

  const updateMenuPosition = (x, y, save) => {
    const clamped = clampMenuPosition(x, y);
    menuX = clamped.x;
    menuY = clamped.y;
    settingsWindow.style.left = `${menuX}px`;
    settingsWindow.style.top = `${menuY}px`;

    if (save && typeof window.tokSaveMenuPosition === 'function') {
      window.tokSaveMenuPosition(`${menuX}|${menuY}`);
    }
  };

  const clearAwaitingBinding = () => {
    awaitingBinding = '';
    Object.values(bindButtons).forEach((button) => button.classList.remove('waiting'));
  };

  const closeAllCustomSelects = (except = null) => {
    document.querySelectorAll('.custom-select.open').forEach((element) => {
      if (element !== except) {
        element.classList.remove('open');
      }
    });
  };

  const setCustomSelectValue = (element, value) => {
    const options = Array.from(element.querySelectorAll('.custom-select-option'));
    const label = element.querySelector('.custom-select-label');
    const target = options.find((option) => option.dataset.value === String(value)) || options[0];
    if (!target) {
      return;
    }
    element.dataset.value = target.dataset.value;
    label.textContent = target.textContent || '';
    options.forEach((option) => option.classList.toggle('selected', option === target));
  };

  const initializeCustomSelect = (element, onChange) => {
    const trigger = element.querySelector('.custom-select-trigger');
    const options = Array.from(element.querySelectorAll('.custom-select-option'));

    trigger.addEventListener('click', (event) => {
      event.preventDefault();
      event.stopPropagation();
      const nextOpen = !element.classList.contains('open');
      closeAllCustomSelects(nextOpen ? element : null);
      element.classList.toggle('open', nextOpen);
    });

    options.forEach((option) => {
      option.addEventListener('click', (event) => {
        event.preventDefault();
        event.stopPropagation();
        const value = option.dataset.value || '0';
        setCustomSelectValue(element, value);
        element.classList.remove('open');
        onChange(value);
      });
    });

    setCustomSelectValue(element, element.dataset.value || '0');
  };

  const beginBinding = (type) => {
    clearAwaitingBinding();
    awaitingBinding = type;
    bindButtons[type].classList.add('waiting');
    bindButtons[type].textContent = 'PRESS KEY';
    if (typeof window.tokBeginBinding === 'function') {
      window.tokBeginBinding(type);
    }
  };

  window.updateWaterHUD = (payload) => {
    const parts = String(payload || '').split('|');
    if (parts.length < 16) {
      return;
    }

    const nextPercent = Number(parts[0]) || 0;
    const nextStage = clampValue(Number(parts[1]) || 0, 0, 4);
    const nextStageName = parts[2] || 'Quenched';
    const hasFilled = parts[3] === '1';
    const isNearWater = parts[4] === '1';
    const nextDifficulty = parts[5] || 'Medium';
    const nextFillKey = parts[6] || 'R';
    const nextHudX = Number(parts[9]) || 24;
    const nextHudY = Number(parts[10]) || 28;
    hudVisible = parts[11] === '1';
    widgetStyle = Number(parts[12]) || 0;
    widgetScale = Number(parts[13]) || 100;
    accessibilityTheme = clampValue(Number(parts[14]) || 0, 0, accessibilityThemes.length - 1);
    waterSystemEnabled = parts[15] === '1';
    currentStage = nextStage;

    applyAccessibilityTheme();
    applyStageColors();
    stage.textContent = nextStageName;
    difficulty.textContent = nextDifficulty;
    percent.textContent = `${nextPercent}%`;
    progress.style.width = `${Math.max(0, Math.min(100, nextPercent))}%`;
    updateWaterSystemButton();
    applyWidgetScale();

    if (nextStage >= 3) {
      panel.classList.add('critical');
    } else {
      panel.classList.remove('critical');
    }

    fillHotkeyKey.textContent = nextFillKey;

    hotkeys.classList.toggle('hidden', !isNearWater);
    fillHotkey.classList.toggle('hidden', !isNearWater);


    applyWidgetStyle();

    if (!hudDragState) {
      updateHudPosition(nextHudX, nextHudY, false);
    }
  };

  window.updateSettingsMenu = (payload) => {
    const parts = String(payload || '').split('|');
    if (parts.length < 13) {
      return;
    }

    if (!awaitingBinding) {
      bindFillButton.textContent = parts[0] || 'R';
      bindMenuButton.textContent = parts[1] || 'F10';
      bindToggleHudButton.textContent = parts[2] || 'H';
    }

    hudVisible = parts[5] === '1';
    widgetStyle = Number(parts[6]) || 0;
    setCustomSelectValue(difficultySelect, Number(parts[7]) || 0);
    setCustomSelectValue(widgetStyleSelect, widgetStyle);
    menuX = Number(parts[8]);
    menuY = Number(parts[9]);
    widgetScale = Number(parts[10]) || 100;
    accessibilityTheme = clampValue(Number(parts[11]) || 0, 0, accessibilityThemes.length - 1);
    waterSystemEnabled = parts[12] === '1';

    setCustomSelectValue(accessibilitySelect, accessibilityTheme);
    applyAccessibilityTheme();
    applyStageColors();
    updateWaterSystemButton();
    applyWidgetScale();
    applyWidgetStyle();
    updateHudPosition(Number(parts[3]) || 24, Number(parts[4]) || 28, false);

    if (!menuDragState) {
      if (menuX < 0 || menuY < 0) {
        centerMenu();
      } else {
        updateMenuPosition(menuX, menuY, false);
      }
    }
  };

  window.setWidgetMenuSuppressed = (payload) => {
    widgetMenuSuppressed = String(payload || '') === '1';
    applyWidgetStyle();
  };

  window.setSettingsMenuOpen = (payload) => {
    settingsOpen = String(payload || '') === '1';
    clearAwaitingBinding();
    closeAllCustomSelects();
    closeDisableConfirm();
    overlay.classList.toggle('hidden', !settingsOpen);
    root.classList.toggle('menu-open', settingsOpen);
    applyWidgetStyle();
    if (settingsOpen) {
      refreshSliderBounds();
      if (typeof window.tokRequestSettings === 'function') {
        window.tokRequestSettings('');
      }
      if (menuX < 0 || menuY < 0) {
        centerMenu();
      } else {
        updateMenuPosition(menuX, menuY, false);
      }
    }
  };

  window.showWaterNotification = (message) => {
    notification.textContent = String(message || '');
    const shouldHide = !waterSystemEnabled || widgetMenuSuppressed || !notification.textContent;
    if (!shouldHide) {
      updateNotificationAnchor(true);
    }
    notification.classList.toggle('hidden', shouldHide);
    notification.style.visibility = '';
    window.clearTimeout(notificationTimer);
    notificationTimer = window.setTimeout(() => {
      notification.textContent = '';
      notification.classList.add('hidden');
      notification.style.visibility = '';
    }, 3000);
  };

  window.finishBinding = () => {
    clearAwaitingBinding();
  };

  window.cancelBinding = () => {
    clearAwaitingBinding();
  };

  closeButton.addEventListener('click', () => {
    if (typeof window.tokCloseSettings === 'function') {
      window.tokCloseSettings('');
    }
  });

  bindFillButton.addEventListener('click', () => beginBinding('fill'));
  bindMenuButton.addEventListener('click', () => beginBinding('menu'));
  bindToggleHudButton.addEventListener('click', () => beginBinding('togglehud'));

  initializeCustomSelect(widgetStyleSelect, (value) => {
    widgetStyle = Number(value) || 0;
    applyWidgetStyle();
    if (typeof window.tokSetWidgetStyle === 'function') {
      window.tokSetWidgetStyle(String(widgetStyle));
    }
  });

  initializeCustomSelect(difficultySelect, (value) => {
    if (typeof window.tokSetDifficulty === 'function') {
      window.tokSetDifficulty(String(value));
    }
  });

  initializeCustomSelect(accessibilitySelect, (value) => {
    accessibilityTheme = clampValue(Number(value) || 0, 0, accessibilityThemes.length - 1);
    applyAccessibilityTheme();
    applyStageColors();
    if (typeof window.tokSetAccessibilityTheme === 'function') {
      window.tokSetAccessibilityTheme(String(accessibilityTheme));
    }
  });

  toggleWaterSystemButton.addEventListener('click', () => {
    if (waterSystemEnabled) {
      disableConfirmOverlay.classList.remove('hidden');
      return;
    }

    if (typeof window.tokSetWaterSystemEnabled === 'function') {
      window.tokSetWaterSystemEnabled('1');
    }
  });

  disableConfirmCancel.addEventListener('click', closeDisableConfirm);
  disableConfirmOverlay.addEventListener('mousedown', (event) => {
    if (event.target === disableConfirmOverlay) {
      closeDisableConfirm();
    }
  });
  disableConfirmAccept.addEventListener('click', () => {
    closeDisableConfirm();
    if (typeof window.tokSetWaterSystemEnabled === 'function') {
      window.tokSetWaterSystemEnabled('0');
    }
  });

  const onSliderInput = () => {
    updateHudPosition(Number(hudXSlider.value), Number(hudYSlider.value), true);
  };
  hudXSlider.addEventListener('input', onSliderInput);
  hudYSlider.addEventListener('input', onSliderInput);

  widgetScaleSlider.addEventListener('input', () => {
    widgetScale = Number(widgetScaleSlider.value) || 100;
    applyWidgetScale();
    if (typeof window.tokSetWidgetScale === 'function') {
      window.tokSetWidgetScale(String(widgetScale));
    }
  });

  resetPositionButton.addEventListener('click', () => {
    updateHudPosition(24, 28, true);
    if (typeof window.tokResetHudPosition === 'function') {
      window.tokResetHudPosition('');
    }
  });

  document.addEventListener('mousedown', (event) => {
    if (!event.target.closest('.custom-select')) {
      closeAllCustomSelects();
    }
  });

  document.addEventListener('keydown', (event) => {
    if (!settingsOpen) {
      return;
    }

    if (awaitingBinding) {
      event.preventDefault();
      event.stopPropagation();

      if (event.code === 'Escape') {
        clearAwaitingBinding();
        if (typeof window.tokCancelBinding === 'function') {
          window.tokCancelBinding('');
        } else if (typeof window.tokRequestSettings === 'function') {
          window.tokRequestSettings('');
        }
      }
      return;
    }

    if (event.code === 'Escape' && !disableConfirmOverlay.classList.contains('hidden')) {
      event.preventDefault();
      event.stopPropagation();
      closeDisableConfirm();
      return;
    }

    if (event.code === 'Escape' && document.querySelector('.custom-select.open')) {
      event.preventDefault();
      event.stopPropagation();
      closeAllCustomSelects();
    }
  }, true);

  const beginHudDrag = (event) => {
    const widget = getActiveWidget();
    if (!settingsOpen || event.button !== 0 || widget.classList.contains('hidden')) {
      return;
    }

    const rect = widget.getBoundingClientRect();
    hudDragState = {
      offsetX: event.clientX - rect.left,
      offsetY: event.clientY - rect.top,
    };
    root.classList.add('dragging');
    event.preventDefault();
    event.stopPropagation();
  };

  panel.addEventListener('mousedown', beginHudDrag);
  simpleWidget.addEventListener('mousedown', beginHudDrag);

  settingsDragHandle.addEventListener('mousedown', (event) => {
    if (!settingsOpen || event.button !== 0) {
      return;
    }

    if (event.target && event.target.closest('button')) {
      return;
    }

    const rect = settingsWindow.getBoundingClientRect();
    menuDragState = {
      offsetX: event.clientX - rect.left,
      offsetY: event.clientY - rect.top,
    };
    settingsWindow.classList.add('dragging');
    event.preventDefault();
  });

  window.addEventListener('mousemove', (event) => {
    if (hudDragState) {
      const rect = getActiveWidgetRect();
      const nextX = event.clientX - hudDragState.offsetX;
      const nextTop = event.clientY - hudDragState.offsetY;
      const nextY = window.innerHeight - nextTop - rect.height;
      updateHudPosition(nextX, nextY, false);
    }

    if (menuDragState) {
      updateMenuPosition(event.clientX - menuDragState.offsetX, event.clientY - menuDragState.offsetY, false);
    }
  });

  window.addEventListener('mouseup', () => {
    if (hudDragState) {
      hudDragState = null;
      root.classList.remove('dragging');
      if (typeof window.tokSaveHudPosition === 'function') {
        window.tokSaveHudPosition(`${hudX}|${hudY}`);
      }
    }

    if (menuDragState) {
      menuDragState = null;
      settingsWindow.classList.remove('dragging');
      if (typeof window.tokSaveMenuPosition === 'function') {
        window.tokSaveMenuPosition(`${menuX}|${menuY}`);
      }
    }
  });

  window.addEventListener('resize', () => {
    refreshSliderBounds();
    updateHudPosition(hudX, hudY, false);
    if (settingsOpen) {
      if (menuX < 0 || menuY < 0) {
        centerMenu();
      } else {
        updateMenuPosition(menuX, menuY, false);
      }
    }
  });

  applyAccessibilityTheme();
  applyStageColors();
  updateNotificationAnchor();
  refreshSliderBounds();
  updateRangeVisual(widgetScaleSlider);
  applyWidgetScale();
  applyWidgetStyle();
})();
