$.get('/fragments/settings/webinterface.html', function(data) {
	$("#settings-pane").html(data);
	$("#webinterface-ssl-generate").click(generatePem);
	
	settingsLoad();
});

function settingsLoad() {
	getSettingsValues(["remote/enable", "remote/port", "remote/password", "remote/ssl", "remote/ssl_pem"], function(hash) {
		$("#webinterface-port").val(hash["remote/port"]);
		$("#webinterface-password").val(hash["remote/password"]);
		$("#webinterface-ssl-pem").val(hash["remote/ssl_pem"]);
		$("#webinterface-ssl").attr('checked', isTrue(hash["remote/ssl"]));
		$("#webinterface-enable").attr('checked', isTrue(hash["remote/enable"]));
	});
}

function settingsSave() {
	port = parseInt($("#webinterface-port").val());
	if (isNaN(port) || port < 1024 || port > 65535)
		throw "Invalid port specified";
	
	setSettingsValue("remote/port", port);
	setSettingsValue("remote/password", $("#webinterface-password").val());
	setSettingsValue("remote/ssl", $("#webinterface-ssl").is(":checked"));
	setSettingsValue("remote/enable", $("#webinterface-enable").is(":checked"));
	setSettingsValue("remote/ssl_pem", $("#webinterface-ssl-pem").val());
}

function generatePem() {
	$("#webinterface-certgen-dlg").dialog({
		resizable: false,
		height:200,
		width:400,
		modal: true,
		open: function() { $("#webinterface-certgen-hostname").val(window.location.hostname); },
		buttons: {
			Cancel: function() {
				$(this).dialog('close');
			},
			OK: function() {
				var hostname = $("#webinterface-certgen-hostname").val();
				if (hostname == "") {
					alert("Enter a valid hostname");
					return;
				}
				
				$.blockUI({ message: 'The certificate is being generated. This may take a while&hellip;' });
				
				var tthis = $(this);
				client.call("HttpService.generateCertificate", hostname, function(data) {
					$("#webinterface-ssl-pem").val(data);
					$.unblockUI();
					tthis.dialog('close');
					alert("The certificate has been generated.");
				});
			}
		}
	});
}
