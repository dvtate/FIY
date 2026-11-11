/////////////////////////////////////////
// Light/dark mode themes
/////////////////////////////////////////

// TODO I'm contemplating just using the media query so that we don't need JS at all

// Dark/light mode switch
const themeBtn = document.getElementById('theme-toggle');

// Change the theme used across the site
function setTheme(dark) {
    document.documentElement.dataset.theme = dark ? 'dark' : 'light';
    if (themeBtn)
        themeBtn.innerText = String.fromCharCode(dark
            ? 9788 : 9790 // '☀' : '☾'
        );
}

// Get theme, with default
let theme = window.localStorage.getItem('theme');
if (theme === null) {
    window.localStorage.setItem('theme', 'auto');
    setTheme(window.matchMedia("(prefers-color-scheme: dark)").matches);
} else {
    setTheme(theme === 'dark');
}

// Toggle theme on click
if (themeBtn)
    themeBtn.onclick = () => {
        const t = window.localStorage.getItem('theme');
        const wasDark = t === 'auto'
            ? window.matchMedia("(prefers-color-scheme: dark)").matches
            : t === 'dark';
        window.localStorage.setItem('theme', wasDark ? 'light' : 'dark');
        setTheme(!wasDark);
    };
