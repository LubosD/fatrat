$.get('/fragments/settings/jabber.html', function(data) {
	$("#settings-pane").html(data);
	$("#jabber-password-enable").click(function() {
		$("#jabber-access-password").attr('disabled', !$(this).is(':checked'));
	});
	
	settingsLoad();
});

function settingsLoad() {
	var keys = ["jabber/enabled", "jabber/jid", "jabber/password", "jabber/resource",
		"jabber/restrict_self", "jabber/restrict_password_bool", "jabber/restrict_password",
		"jabber/grant_auth", "jabber/priority"
	];
	
	getSettingsValues(keys, function(hash) {
		$("#jabber-enable").attr('checked', isTrue(hash["jabber/enabled"]));
		$("#jabber-authonrequest").attr('checked', isTrue(hash["jabber/grant_auth"]));
		$("#jabber-restrict-access").attr('checked', isTrue(hash["jabber/restrict_self"]));
		$("#jabber-password-enable").attr('checked', isTrue(hash["jabber/restrict_password_bool"]));
		$("#jabber-access-password").attr('disabled', !isTrue(hash["jabber/restrict_password_bool"]));
		$("#jabber-jid").val(hash["jabber/jid"]);
		$("#jabber-password").val(hash["jabber/password"]);
		$("#jabber-priority").val(hash["jabber/priority"]);
		$("#jabber-resource").val(hash["jabber/resource"]);
		$("#jabber-access-password").val(hash["jabber/restrict_password"]);
	});
}

function settingsSave() {
	setSettingsValue("jabber/enabled", $("#jabber-enable").is(":checked"));
	setSettingsValue("jabber/restrict_password_bool", $("#jabber-password-enable").is(":checked"));
	setSettingsValue("jabber/restrict_self", $("#jabber-restrict-access").is(":checked"));
	setSettingsValue("jabber/grant_auth", $("#jabber-authonrequest").is(":checked"));
	setSettingsValue("jabber/jid", $("#jabber-jid").val());
	setSettingsValue("jabber/password", $("#jabber-password").val());
	setSettingsValue("jabber/priority", checkInt("jabber-priority"));
	setSettingsValue("jabber/resource", $("#jabber-resource").val());
	setSettingsValue("jabber/restrict_password", $("#jabber-access-password").val());
}
