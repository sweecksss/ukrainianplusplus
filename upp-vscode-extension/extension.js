const vscode = require('vscode');
const path = require('path');

const COMPLETIONS = [
  // Ключові слова
  { label: 'нехай', kind: vscode.CompletionItemKind.Keyword, detail: 'оголосити змінну' },
  { label: 'буде', kind: vscode.CompletionItemKind.Keyword },
  { label: 'стає', kind: vscode.CompletionItemKind.Keyword, detail: 'змінити наявну змінну' },
  { label: 'показати', kind: vscode.CompletionItemKind.Keyword, detail: 'вивести значення' },
  { label: 'якщо', kind: vscode.CompletionItemKind.Keyword },
  { label: 'інакше', kind: vscode.CompletionItemKind.Keyword },
  { label: 'поки', kind: vscode.CompletionItemKind.Keyword },
  { label: 'то', kind: vscode.CompletionItemKind.Keyword },
  { label: 'функція', kind: vscode.CompletionItemKind.Keyword, detail: 'оголосити функцію' },
  { label: 'повернути', kind: vscode.CompletionItemKind.Keyword, detail: 'повернути значення' },

  // Літерали
  { label: 'правда', kind: vscode.CompletionItemKind.Constant },
  { label: 'брехня', kind: vscode.CompletionItemKind.Constant },

  // Арифметика
  { label: 'додати', kind: vscode.CompletionItemKind.Operator, detail: 'a + b' },
  { label: 'відняти', kind: vscode.CompletionItemKind.Operator, detail: 'a - b' },
  { label: 'помножити', kind: vscode.CompletionItemKind.Operator, detail: 'a * b' },
  { label: 'поділити', kind: vscode.CompletionItemKind.Operator, detail: 'a / b' },

  // Порівняння
  { label: 'більше', kind: vscode.CompletionItemKind.Operator, detail: 'a > b' },
  { label: 'менше', kind: vscode.CompletionItemKind.Operator, detail: 'a < b' },
  { label: 'дорівнює', kind: vscode.CompletionItemKind.Operator, detail: 'a == b' },
  { label: 'не дорівнює', kind: vscode.CompletionItemKind.Operator, detail: 'a != b' },
  { label: 'не менше', kind: vscode.CompletionItemKind.Operator, detail: 'a >= b' },
  { label: 'не більше', kind: vscode.CompletionItemKind.Operator, detail: 'a <= b' },

  // Логіка
  { label: 'і', kind: vscode.CompletionItemKind.Operator, detail: 'логічне І' },
  { label: 'та', kind: vscode.CompletionItemKind.Operator, detail: 'логічне І (синонім)' },
  { label: 'або', kind: vscode.CompletionItemKind.Operator, detail: 'логічне АБО' },
  { label: 'не', kind: vscode.CompletionItemKind.Operator, detail: 'заперечення' },

  // Вбудовані функції
  { label: 'довжина', kind: vscode.CompletionItemKind.Function, detail: 'довжина(рядок або список)' },
  { label: 'ввести', kind: vscode.CompletionItemKind.Function, detail: 'ввести() або ввести(підказка)' },
  { label: 'число', kind: vscode.CompletionItemKind.Function, detail: 'число(значення)' },
  { label: 'текст', kind: vscode.CompletionItemKind.Function, detail: 'текст(значення)' },
  { label: 'додати_до', kind: vscode.CompletionItemKind.Function, detail: 'додати_до(список, значення)' }
];

/**
 * @param {vscode.ExtensionContext} context
 */
function activate(context) {
  const runCommand = vscode.commands.registerCommand('upp.runFile', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
      vscode.window.showErrorMessage('Нет активного редактора для запуска U++ файла.');
      return;
    }

    const document = editor.document;

    if (document.languageId !== 'upp') {
      vscode.window.showErrorMessage('Команда "Run U++" работает только для файлов U++ (.upp).');
      return;
    }

    if (document.isUntitled) {
      const save = await document.save();
      if (!save) {
        vscode.window.showWarningMessage('Сохраните файл перед запуском программы U++.'); 
        return;
      }
    } else {
      await document.save();
    }

    const filePath = document.fileName;
    const workspaceFolder = vscode.workspace.getWorkspaceFolder(document.uri);
    const cwd = workspaceFolder ? workspaceFolder.uri.fsPath : path.dirname(filePath);

    const terminalName = 'U++ Run';
    let terminal = vscode.window.terminals.find(t => t.name === terminalName);
    if (!terminal) {
      terminal = vscode.window.createTerminal({ name: terminalName, cwd });
    }

    terminal.show(true);

    const relativePath = workspaceFolder ? path.relative(cwd, filePath) : filePath;
    const fs = require('fs');
    const isWindows = process.platform === 'win32';
    const exeName = isWindows ? 'upp.exe' : 'upp';

    // Свіжозібраний бінарник у upp-c/ має перевагу над копією в корені,
    // інакше після перезбирання запускалася б стара версія.
    const localCandidates = workspaceFolder
      ? [path.join(cwd, 'upp-c', exeName), path.join(cwd, exeName)]
      : [];
    const found = localCandidates.find(candidate => fs.existsSync(candidate));

    let command;
    if (found) {
      const relativeExe = path.relative(cwd, found);
      command = isWindows
        ? `.\\${relativeExe} "${relativePath}"`
        : `./${relativeExe} "${relativePath}"`;
    } else {
      // Інсталятор додає upp у PATH; якщо його там немає, лишається обгортка.
      command = `${exeName} "${relativePath}"`;
    }

    terminal.sendText(command);
  });

  const completionProvider = vscode.languages.registerCompletionItemProvider(
    { language: 'upp' },
    {
      provideCompletionItems() {
        return COMPLETIONS.map(({ label, kind }) => {
          const item = new vscode.CompletionItem(label, kind);
          item.insertText = label;
          return item;
        });
      }
    }
  );

  context.subscriptions.push(runCommand, completionProvider);
}

function deactivate() {}

module.exports = {
  activate,
  deactivate
};

