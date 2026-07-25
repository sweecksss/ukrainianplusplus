function showTab(tabId) {
    // Hide all panels
    const panels = document.querySelectorAll('.tab-panel');
    panels.forEach(p => p.classList.remove('active'));

    // Remove active class from buttons
    const buttons = document.querySelectorAll('.doc-tab');
    buttons.forEach(b => b.classList.remove('active'));

    // Show target panel
    const targetPanel = document.getElementById(tabId);
    if (targetPanel) {
        targetPanel.classList.add('active');
    }

    // Highlight button
    event.currentTarget.classList.add('active');
}

function copyHeroCode() {
    const codeText = `# Програма на мові U++
нехай а буде 10
нехай б буде 10

якщо а дорівнює б то
    показати "Привіт, Україно! 🎉"`;

    navigator.clipboard.writeText(codeText).then(() => {
        const btn = document.querySelector('.copy-btn');
        const orig = btn.textContent;
        btn.textContent = 'Скопійовано! ✓';
        btn.style.color = '#34d399';
        setTimeout(() => {
            btn.textContent = orig;
            btn.style.color = '';
        }, 2000);
    });
}
