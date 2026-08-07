; Anabasis Windows installer (Inno Setup 6 — preinstalled on windows-latest).
; Compiled from the validated staging directory:
;   ISCC.exe /DAppVersion=<x.y.z> /DStagingDir=<abs path to dist\Anabasis-Windows> /O<outdir> packaging\windows\Anabasis.iss
; StagingDir is the CI-validated customer payload (Anabasis.vst3\ bundle +
; Anabasis.exe, PDB-purged) produced by build.yml's "Stage Windows artifacts"
; step — the installer repacks those exact files.
; The installer is NOT Authenticode-signed yet. (The sibling names the release-
; hardening task that will sign the same exe; Anabasis has no such task ID, and
; inventing one would be a fabricated reference, so the pointer is dropped.)
;
; Wizard flow: component selection (Install VST3 / Install Standalone, both
; pre-selected, at least one required) → one destination page carrying BOTH
; install paths (VST3 folder above the Standalone folder) → install. The
; standard single-directory page is disabled; the custom destination page
; below replaces it, and the chosen Standalone folder is written back to
; {app} so the uninstaller, Start-menu icon and registry entry stay coherent.

#ifndef AppVersion
  #error AppVersion must be passed with /DAppVersion=x.y.z
#endif
#ifndef StagingDir
  #error StagingDir must be passed with /DStagingDir=<staged customer dir>
#endif

[Setup]
; ArchitecturesAllowed/InstallIn64BitMode use the `x64compatible` identifier,
; which requires Inno Setup >= 6.3 (2024). The sibling additionally records the
; compiler engine its CI proved this script against; that claim is dropped here
; rather than copied, because Anabasis's build.yml has no Inno Setup step to
; cite as evidence — a compile-validation claim with no run behind it is worse
; than none.
; Stable AppId: upgrades and uninstalls must always target the same product.
; This GUID is ANABASIS'S OWN and must NEVER change once a build has shipped —
; Inno keys the uninstall/upgrade entry on it, so a changed AppId turns an
; upgrade into a second, separately-uninstallable product. It must also never
; be the sibling's AppId: sharing one would make either installer uninstall the
; other's files.
AppId={{19AAA915-4527-4BA2-B6A7-06068E527C54}
AppName=Anabasis
AppVersion={#AppVersion}
AppPublisher=RollyTech
AppPublisherURL=https://www.rolly.tech
DefaultDirName={autopf}\Anabasis
DefaultGroupName=Anabasis
DisableProgramGroupPage=yes
; The standard directory page only knows ONE path ({app}) and never says what
; it is for; the custom destination page in [Code] shows both clearly-labelled
; paths (VST3 first) instead.
DisableDirPage=yes
OutputBaseFilename=Anabasis-{#AppVersion}-Windows-Installer
Compression=lzma2
SolidCompression=yes
; The CI build is x64-only; install the VST3 into the 64-bit Common Files tree.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; {commoncf64}\VST3 requires elevation.
PrivilegesRequired=admin
UninstallDisplayIcon={app}\Anabasis.exe
WizardStyle=modern

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "Install VST3"; Types: full custom
Name: "standalone"; Description: "Install Standalone"; Types: full custom

[Files]
Source: "{#StagingDir}\Anabasis.vst3\*"; DestDir: "{code:GetVst3Dir}\Anabasis.vst3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#StagingDir}\Anabasis.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
; The installer payload is deliberately lean: only what the user needs to run
; the product -- it installs no attribution file, and INSTALL.txt (which this
; installer does not install either) is installation-only.
; NO DEVIATION from the sibling here, and the note that used to claim one is
; gone with the behaviour it described. Since ADR-0021 the StagingDir does NOT
; contain NOTICE or THIRD_PARTY_LICENSES.md either -- build.yml's "Stage
; Windows artifacts" step stopped copying them -- and RELEASE_POLICY.md's
; "Third-party attribution" section was amended by that ADR to say the
; requirement is satisfied by VERSION-NAMED RELEASE-PAGE ASSETS
; (Anabasis-<version>-NOTICE.txt and -THIRD_PARTY_LICENSES.md), which is the
; one carrier every distribution route passes through: this installer, the
; .pkg and the three zips alike. So the earlier note's closing worry -- "a user
; who downloads ONLY the installer gets no attribution carrier at all" -- is
; answered rather than outstanding: the carrier is on the page they downloaded
; it from. The sibling's SUPPORT.md mention stays dropped from the payload;
; Anabasis's SUPPORT.md is likewise a release-page asset.

[Icons]
Name: "{group}\Anabasis"; Filename: "{app}\Anabasis.exe"; Components: standalone
Name: "{group}\Uninstall Anabasis"; Filename: "{uninstallexe}"

[Code]
var
  DestPage: TInputDirWizardPage;

procedure InitializeWizard;
begin
  // One destination page for both components, shown right after the component
  // page. Index 0 (top) = VST3 folder, index 1 = Standalone folder.
  // NOTE: comments in this section are //-style throughout — Pascal { } comments
  // do not nest, so a literal constant name like the ones expanded below would
  // terminate a brace comment early and break the ISCC compile.
  DestPage := CreateInputDirPage(wpSelectComponents,
    'Select Destination Locations',
    'Where should the selected components be installed?',
    'Setup will install each selected component into its folder below.' + #13#10 +
    'To continue, click Next. To pick different folders, click Browse.',
    False, 'Anabasis');
  DestPage.Add('VST3 plug-in folder (the plug-in installs as Anabasis.vst3 inside it):');
  DestPage.Add('Standalone application folder:');
  DestPage.Values[0] := ExpandConstant('{commoncf64}\VST3');
  DestPage.Values[1] := WizardDirValue;   // previous install dir, else the DefaultDirName default
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  // Only the paths of selected components are editable.
  if (DestPage <> nil) and (CurPageID = DestPage.ID) then
  begin
    DestPage.PromptLabels[0].Enabled := WizardIsComponentSelected('vst3');
    DestPage.Edits[0].Enabled        := WizardIsComponentSelected('vst3');
    DestPage.Buttons[0].Enabled      := WizardIsComponentSelected('vst3');
    DestPage.PromptLabels[1].Enabled := WizardIsComponentSelected('standalone');
    DestPage.Edits[1].Enabled        := WizardIsComponentSelected('standalone');
    DestPage.Buttons[1].Enabled      := WizardIsComponentSelected('standalone');
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectComponents then
  begin
    // At least one component must be selected before continuing.
    if WizardSelectedComponents(False) = '' then
    begin
      MsgBox('Select at least one component to install.', mbError, MB_OK);
      Result := False;
    end;
  end
  else if (DestPage <> nil) and (CurPageID = DestPage.ID) then
  begin
    if WizardIsComponentSelected('vst3') and (Trim(DestPage.Values[0]) = '') then
    begin
      MsgBox('Enter a folder for the VST3 plug-in.', mbError, MB_OK);
      Result := False;
      exit;
    end;
    if WizardIsComponentSelected('standalone') and (Trim(DestPage.Values[1]) = '') then
    begin
      MsgBox('Enter a folder for the Standalone application.', mbError, MB_OK);
      Result := False;
      exit;
    end;
    // Feed the chosen Standalone folder back into the app directory constant
    // (the dir page is disabled, so this edit is the only writer). The
    // uninstaller and the Start-menu icon resolve against it.
    WizardForm.DirEdit.Text := DestPage.Values[1];
  end;
end;

function GetVst3Dir(Param: string): string;
begin
  Result := DestPage.Values[0];
end;
