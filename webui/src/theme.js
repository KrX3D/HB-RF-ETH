// Colour theme: "light", "dark" or "system" (follows prefers-color-scheme).
// The choice is stored in localStorage; the effective theme is set as
// data-theme on <html>, theme.css styles html[data-theme="dark"].

const STORAGE_KEY = "hb-rf-eth-theme";
export const THEME_MODES = ["light", "dark", "system"];

const media = window.matchMedia ? window.matchMedia("(prefers-color-scheme: dark)") : null;

export function loadThemeMode() {
  const mode = localStorage.getItem(STORAGE_KEY);
  return THEME_MODES.includes(mode) ? mode : "system";
}

export function applyTheme(mode) {
  const dark = mode == "dark" || (mode == "system" && media != null && media.matches);
  document.documentElement.setAttribute("data-theme", dark ? "dark" : "light");
}

export const moduleTheme = {
  namespaced: true,
  state: () => ({
    mode: loadThemeMode()
  }),
  mutations: {
    setMode(state, mode) {
      state.mode = mode;
      localStorage.setItem(STORAGE_KEY, mode);
      applyTheme(mode);
    }
  },
  actions: {
    cycle(context) {
      const next = THEME_MODES[(THEME_MODES.indexOf(context.state.mode) + 1) % THEME_MODES.length];
      context.commit("setMode", next);
    }
  }
};

export function initTheme(store) {
  applyTheme(store.state.theme.mode);
  if (media != null) {
    const onChange = () => applyTheme(store.state.theme.mode);
    // Safari < 14 only knows addListener
    if (media.addEventListener) media.addEventListener("change", onChange);
    else if (media.addListener) media.addListener(onChange);
  }
}
