// Apply the saved palette before the page paints; wire up the toggle after parsing.
(() => {
    const preference = window.matchMedia('(prefers-color-scheme: dark)');
    let theme = 'auto';
    try { theme = localStorage.getItem('theme') || 'auto'; } catch (_) {}
    function applyTheme() {
        const dark = theme === 'dark' || (theme !== 'light' && preference.matches);
        document.documentElement.dataset.theme = dark ? 'dark' : 'light';
        const button = document.getElementById('theme-toggle');
        if (button) {
            button.textContent = dark ? '☀' : '☾';
            button.setAttribute('aria-label', dark ? 'Switch to light theme' : 'Switch to dark theme');
        }
    }
    applyTheme();
    document.addEventListener('DOMContentLoaded', () => {
        applyTheme();
        document.getElementById('theme-toggle')?.addEventListener('click', () => {
            theme = document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark';
            try { localStorage.setItem('theme', theme); } catch (_) {}
            applyTheme();
        });
    });
    preference.addEventListener('change', applyTheme);
    window.addEventListener('storage', event => {
        if (event.key === 'theme' || event.key === null) {
            theme = event.newValue || 'auto';
            applyTheme();
        }
    });
})();
