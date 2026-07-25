const vscode = require('vscode');
const path = require('path');

const COMPLETIONS = [
  // Keywords
  { label: 'нехай', kind: vscode.CompletionItemKind.Keyword },
  { label: 'буде', kind: vscode.CompletionItemKind.Keyword },
  { label: 'показати', kind: vscode.CompletionItemKind.Keyword },
  { label: 'якщо', kind: vscode.CompletionItemKind.Keyword },
  { label: 'інакше', kind: vscode.CompletionItemKind.Keyword },
  { label: 'поки', kind: vscode.CompletionItemKind.Keyword },
  { label: 'то', kind: vscode.CompletionItemKind.Keyword },

  // Boolean literals
  { label: 'правда', kind: vscode.CompletionItemKind.Constant },
  { label: 'брехня', kind: vscode.CompletionItemKind.Constant },

  // Operators
  { label: 'додати', kind: vscode.CompletionItemKind.Operator },
  { label: 'відняти', kind: vscode.CompletionItemKind.Operator },
  { label: 'помножити', kind: vscode.CompletionItemKind.Operator },
  { label: 'поділити', kind: vscode.CompletionItemKind.Operator },
  { label: 'більше', kind: vscode.CompletionItemKind.Operator },
  { label: 'менше', kind: vscode.CompletionItemKind.Operator },
  { label: 'дорівнює', kind: vscode.CompletionItemKind.Operator },
  { label: 'і', kind: vscode.CompletionItemKind.Operator },
  { label: 'або', kind: vscode.CompletionItemKind.Operator },
  { label: 'не', kind: vscode.CompletionItemKind.Operator }
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
    const exePath = workspaceFolder ? path.join(cwd, 'upp.exe') : 'upp.exe';
    const command = fs.existsSync(exePath) ? `.\\upp.exe "${relativePath}"` : `python main.py "${relativePath}"`;

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

