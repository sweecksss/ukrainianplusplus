#!/usr/bin/env python3
"""Збирає .vsix розширення для VS Code без зовнішніх інструментів.

.vsix — це звичайний zip із маніфестом. Робимо його вручну, щоб
перезбирання не потребувало ані мережі, ані встановленого vsce, і щоб
версія в package.json і в маніфесті не розʼїжджалися.

    python tools/build_vsix.py
"""

from __future__ import annotations

import json
import re
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXT = ROOT / "upp-vscode-extension"

# Що саме потрапляє в пакет. Порядок збережено таким, як його робить vsce.
FILES = [
    ("package.json", "extension/package.json"),
    ("README.md", "extension/readme.md"),
    ("language-configuration.json", "extension/language-configuration.json"),
    ("extension.js", "extension/extension.js"),
    ("snippets/upp.code-snippets", "extension/snippets/upp.code-snippets"),
    ("syntaxes/upp.tmLanguage.json", "extension/syntaxes/upp.tmLanguage.json"),
    ("fileicons/upp-icon-theme.json", "extension/fileicons/upp-icon-theme.json"),
    ("fileicons/upp-file-icon.svg", "extension/fileicons/upp-file-icon.svg"),
]

CONTENT_TYPES = """<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
	<Default Extension="json" ContentType="application/json" />
	<Default Extension="vsixmanifest" ContentType="text/xml" />
	<Default Extension="md" ContentType="text/markdown" />
	<Default Extension="js" ContentType="application/javascript" />
	<Default Extension="svg" ContentType="image/svg+xml" />
	<Default Extension="code-snippets" ContentType="application/json" />
</Types>
"""

MANIFEST = """<?xml version="1.0" encoding="utf-8"?>
	<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011" xmlns:d="http://schemas.microsoft.com/developer/vsx-schema-design/2011">
		<Metadata>
			<Identity Language="en-US" Id="{name}" Version="{version}" Publisher="{publisher}" />
			<DisplayName>{display_name}</DisplayName>
			<Description xml:space="preserve">{description}</Description>
			<Tags>theme,icon-theme,snippet,upp,U,UkrainianPlusPlus,__ext_upp</Tags>
			<Categories>Programming Languages</Categories>
			<GalleryFlags>Public</GalleryFlags>
			<Properties>
				<Property Id="Microsoft.VisualStudio.Code.Engine" Value="{engine}" />
				<Property Id="Microsoft.VisualStudio.Code.ExtensionDependencies" Value="" />
				<Property Id="Microsoft.VisualStudio.Code.ExtensionPack" Value="" />
				<Property Id="Microsoft.VisualStudio.Code.ExtensionKind" Value="workspace" />
				<Property Id="Microsoft.VisualStudio.Code.LocalizedLanguages" Value="" />
				<Property Id="Microsoft.VisualStudio.Code.EnabledApiProposals" Value="" />
				<Property Id="Microsoft.VisualStudio.Code.ExecutesCode" Value="true" />
				<Property Id="Microsoft.VisualStudio.Services.GitHubFlavoredMarkdown" Value="true" />
				<Property Id="Microsoft.VisualStudio.Services.Content.Pricing" Value="Free"/>
			</Properties>
		</Metadata>
		<Installation>
			<InstallationTarget Id="Microsoft.VisualStudio.Code"/>
		</Installation>
		<Dependencies/>
		<Assets>
			<Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true" />
			<Asset Type="Microsoft.VisualStudio.Services.Content.Details" Path="extension/readme.md" Addressable="true" />
		</Assets>
	</PackageManifest>
"""


def xml_escape(text: str) -> str:
    return (text.replace("&", "&amp;")
                .replace("<", "&lt;")
                .replace(">", "&gt;")
                .replace('"', "&quot;"))


def main() -> int:
    pkg = json.loads((EXT / "package.json").read_text(encoding="utf-8"))

    version = pkg["version"]
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        print(f"Некоректна версія в package.json: {version}", file=sys.stderr)
        return 1

    manifest = MANIFEST.format(
        name=xml_escape(pkg["name"]),
        version=xml_escape(version),
        publisher=xml_escape(pkg["publisher"]),
        display_name=xml_escape(pkg["displayName"]),
        description=xml_escape(pkg["description"]),
        engine=xml_escape(pkg["engines"]["vscode"]),
    )

    missing = [src for src, _ in FILES if not (EXT / src).exists()]
    if missing:
        for name in missing:
            print(f"Не знайдено {name}", file=sys.stderr)
        return 1

    targets = [
        EXT / f"upp-vscode-extension-{version}.vsix",
        ROOT / "downloads" / "upp-vscode-extension.vsix",
    ]

    for target in targets:
        target.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as z:
            z.writestr("extension.vsixmanifest", manifest)
            z.writestr("[Content_Types].xml", CONTENT_TYPES)
            for src, dst in FILES:
                z.write(EXT / src, dst)
        print(f"Зібрано {target.relative_to(ROOT)}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
