program SmartConfigController;

uses
  Vcl.Forms,
  uRelayController in 'uRelayController.pas' {FormSmartConfig};

{$R *.res}

begin
  Application.Initialize;
  Application.MainFormOnTaskbar := True;
  Application.Title := 'SmartConfig Relay Controller';
  Application.CreateForm(TFormSmartConfig, FormSmartConfig);
  Application.Run;
end.
