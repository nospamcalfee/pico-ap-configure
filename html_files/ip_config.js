function ValidateIPaddress(textbox) {
	var ipformat = /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;
	if(textbox.value.match(ipformat)) {
		textbox.focus();
		return true;
	} else {
		alert("You have entered an invalid IP address!");
		textbox.focus();return false;
	}
}
