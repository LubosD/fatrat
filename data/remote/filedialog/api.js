
var lastDirElem;

function dirDialog(elemId)
{
	var elem = document.getElementById(elemId);
	var win = window.open('/filedialog/dirdialog.html?'+elem.value, 'dirdialog', 'width=660,height=500,toolbar=0');
	lastDirElem = elem;
}

function doneDirDialog(path)
{
	lastDirElem.value = path;
}
