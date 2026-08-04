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

    // Highlight the button that points at this panel. Раніше тут був
    // глобальний event.currentTarget — він працює лише всередині
    // inline-обробника і падає, якщо showTab викликати з коду.
    buttons.forEach(b => {
        const onclick = b.getAttribute('onclick') || '';
        if (onclick.includes(`'${tabId}'`)) {
            b.classList.add('active');
        }
    });
}

function copyHeroCode() {
    // Беремо текст із самого блоку, а не з копії в коді: інакше кнопка
    // копіює застарілий приклад щоразу, коли правлять розмітку.
    const codeElement = document.getElementById('hero-code');
    if (!codeElement) return;

    const codeText = codeElement.textContent;
    const btn = document.querySelector('.copy-btn');

    navigator.clipboard.writeText(codeText).then(() => {
        if (!btn) return;
        const orig = btn.textContent;
        btn.textContent = 'Скопійовано! ✓';
        btn.style.color = '#34d399';
        setTimeout(() => {
            btn.textContent = orig;
            btn.style.color = '';
        }, 2000);
    }).catch(() => {
        if (!btn) return;
        btn.textContent = 'Не вдалося скопіювати';
        setTimeout(() => {
            btn.textContent = 'Копіювати';
        }, 2000);
    });
}

function openDownloadModal() {
    const modal = document.getElementById('download-modal');
    if (modal) {
        modal.classList.add('active');
    }
}

function closeDownloadModal() {
    const modal = document.getElementById('download-modal');
    if (modal) {
        modal.classList.remove('active');
    }
}

function copyLinuxCmd() {
    const cmdText = "curl -fsSL https://raw.githubusercontent.com/sweecksss/ukrainianplusplus/main/install.sh | bash";
    const btn = document.getElementById('copy-linux-btn');

    navigator.clipboard.writeText(cmdText).then(() => {
        if (!btn) return;
        btn.textContent = 'Скопійовано! ✓';
        setTimeout(() => {
            btn.textContent = 'Скопіювати';
        }, 2000);
    });
}

document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') closeDownloadModal();
});
