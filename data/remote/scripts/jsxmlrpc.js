/* 
	Copyright (c) 2006, Tim Becker (tim@kuriositaet.de)
	All rights reserved.
	Full license details at www.kurisositaet.de/jsxmlrpc/LICENSE.txt
	In short: feel free to do what you want with this, but give me some 
	credit and don't sue me. (BSD License)
*/
	







function getXMLHttpRequest () {
 var xmlhttp = false;
 var arr = [
 function(){return new XMLHttpRequest();},
 function(){return new ActiveXObject("Microsoft.XMLHTTP");},
 function(){return new ActiveXObject("Msxml2.XMLHTTP");}
 ]
  for (var i=0; i!=arr.length; ++i) {
 try { 
 xmlhttp = arr[i]()
 break
 } catch (e){} 
 }
 return xmlhttp;
}


function checkRequestStatus (request) {
  if (request.status != 200 && request.status != 0) {
 var exception = request.status.toString() + " : "
 exception += request.statusText ? request.statusText : "Network error"
 exception.errCode = request.status
 throw exception
 }
}

function readyStateChangeFunc (request, lambda) {
 return function () {
 if (request.readyState != Request.COMPLETED) return
 try {
 checkRequestStatus(request)
 lambda(request) 
 } catch (e) { request.onnetworkerror(e) }
 }
}

function getOnreadystatechangeCallback (req){
 arr = ["readyState", "responseBody", "responseStream", "responseText", "responseXML", "status", "statusText"]
 return function () {
 for (var i=0; i!= arr.length; ++i) {
 try {
 req[arr[i]]=req._request[arr[i]] 
 } catch (e){} 
 }
 if (req.onreadystatechange){
 req.onreadystatechange()
 }
 }
}




function Request (url, content, callback) {
 this.url = url
 this.content = content
 this.callback = callback
 this.contentType = "text/xml"

 this.requestMethod = null
 
 
 this.readyState = 0
 this.responseBody = null
 this.responseStream = null
 this.responseText = null
 this.responseXML = null
 this.status = null
 this.statusText = null

 this._request = getXMLHttpRequest()
 this._request.onreadystatechange = getOnreadystatechangeCallback(this)
 this.abort = function () {
 return this._request.abort()
 }
 this.getAllResponseHeaders = function () {
 return this._request.getAllResponseHeaders()
 }
 this.getResponseHeader = function (str) {
 return this._request.getResponseHeader(str) 
 }
 this.open = function (method, url, async, user, password) {
 if (typeof(async)=="undefined") async = true
 if (typeof(user)=="undefined") user = null
 if (typeof(password)=="undefined") password = null
 return this._request.open(method, url, async, user, password) 
 }
 this.send = function (content) {
 if (typeof(content)=="undefined") content = "" 
 var tmp = this._request.send(content) 
 
 }
 this.setRequestHeader = function (name, value) {
 return this._request.setRequestHeader(name, value) 
 }

 this.copyAttributes = function () {
 arr = ["readyState", "responseBody", "responseStream", "responseText", "responseXML", "status", "statusText"]
 for (var i=0; i!= arr.length; ++i) {
 try {
 this[arr[i]]=this._request[arr[i]] 
 } catch (e){} 
 }

 }
  

 this.process = function () {
 if (callback){
 this._request.onnetworkerror = this.onnetworkerror
 this._request.onreadystatechange = readyStateChangeFunc (this._request, callback)
 }
 var async = callback ? true : false
 if (!this.requestMethod) {
 if (!this.content){
 this.requestMethod="GET" 
 } else {
 this.requestMethod="POST" 
 }
 }
 this.open (this.requestMethod, this.url, async)
 this.setRequestHeader("Content-Length", this.content!=null?this.content.length:0)
 if (this.content!=null)
 this.setRequestHeader("Content-Type",this.contentType);
 this.send (this.content)
 if (async) return

 this.copyAttributes()
 checkRequestStatus(this) 
 }
 this.onnetworkerror = function (e) {
	showError(e);
 }
}

Request.UNINITIALIZED = 0
Request.LOADING = 1
Request.LOADED = 2
Request.INTERACTIVE = 3
Request.COMPLETED = 4









function isA (obj, type) {
 return obj.constructor.prototype == type.prototype
}

function isInt (obj) {
 if (!isA(obj,Number)) return false
 return (obj % 1) == 0
}

function isFloat(obj) {
 if (!isA(obj,Number)) return false
 return !isInt(obj)
}





if (!window.Node) {
 window.Node = { 
 ELEMENT_NODE: 1,
 ATTRIBUTE_NODE: 2,
 TEXT_NODE: 3,
 CDATA_SECTION_NODE: 4,
 ENTITY_REFERENCE_NODE: 5,
 ENTITY_NODE: 6,
 PROCESSING_INSTRUCTION_NODE: 7,
 COMMENT_NODE: 8,
 DOCUMENT_NODE: 9,
 DOCUMENT_TYPE_NODE: 10,
 DOCUMENT_FRAGMENT_NODE: 11,
 NOTATION_NODE: 12
 }
} 

function makeTag (tagName, strValue) {
 return "<"+tagName+">"+strValue+"</"+tagName+">"
}




function visitChildren (node, lamdba, optionalNodeTypeRestriction) {
 var nodeList = node.childNodes
 eachInNodeList(nodeList, lamdba, optionalNodeTypeRestriction)
}


function eachInNodeList (nodeList, lambda, optionalNodeTypeRestriction) {
 for (var i=0; i!=nodeList.length; ++i) {
 if ( optionalNodeTypeRestriction && 
 nodeList[i].nodeType!=optionalNodeTypeRestriction) 
 {
 continue
 }
 lambda (nodeList[i])
 }

}


function getTextValueOfChild (node) {
 var str
 visitChildren ( 
 node,
 function (x){str=x.nodeValue},
 Node.TEXT_NODE
 )
 if (!str){
 str = "" 
 }
 str = str.replace(/^\s+/,"") 
 return str.replace(/\s+$/, "")
}


function getNamedChild (node, name) {
 var ret
 visitChildren (
 node,
 function(x){if(x.nodeName == name) ret = x},
 Node.ELEMENT_NODE
 )
 return ret
}




function getNamedChildren (node, name){
 var ret=[]
 visitChildren (
 node,
 function(x){if(x.nodeName == name) ret.push(x)},
 Node.ELEMENT_NODE
 )
 return ret
}








function encodeXmlRpc(arg) {

 if (isA(arg,String)){
 arg = arg.replace(/&/g, "&amp;")
 arg = arg.replace(/</g, "&lt;")
 arg = arg.replace(/>/g, "&gt;")
 return makeTag("string", arg)
 }else if (isA(arg,Boolean)){
 return makeTag("boolean", (arg?"1":"0"))
 }else if (isA(arg,Date)){
 return makeTag("dateTime.iso8601", getIso8601Str(arg)) 
 }else if (isA(arg,Array)){
 return makeTag("array", makeTag("data", makeArrayValues(arg)))
 }else if (isA(arg,Number)){
 if (isInt(arg)){
 return makeTag("int",arg) 
 } 
 return makeTag("double", arg)
 }else{
  return makeTag("struct", makeStructMembers(arg))
 }
}


function makeArrayValues (arg) {
 var str = "";
 for (var i=0; i!=arg.length; ++i){
 str += makeTag("value", encodeXmlRpc(arg[i]))
 }
 return str
}


function makeStructMembers (arg) {
 var str = ""
 for (var p in arg){
 if (isA(arg[p],Function)) continue;
 str += makeTag("member", makeTag("name",p)+makeTag("value", encodeXmlRpc(arg[p]))) 
 }
 return str
}


function getIso8601Str(date){
 var str = date.getFullYear().toString()
 var month = date.getMonth()+1
 if (month<10) str += "0"
 str += month
 day = date.getDate()
 if (day<10) str += "0"
 str += day
 str += "T"+date.getHours()+":"+date.getMinutes()+":"+date.getSeconds();
  return str
}







function getDateFromChild (dateNode) {
 var dateString = getTextValueOfChild(dateNode)
  var re = /(\d\d\d\d)(\d\d)(\d\d)T(\d\d):(\d\d):(\d\d)/
 var result = re.exec (dateString)
 var month=new Number(result[2])
 return new Date(result[1],month-1,result[3],result[4],result[5],result[6])
}

function getArray (arrayNode) {
 var dataNode = getNamedChild(arrayNode, "data") 
 var valueNodes= getNamedChildren (dataNode, "value")
 var result=[]
 eachInNodeList(
 valueNodes,
 function(x){result.push(getResultFromValueNode(x))},
 Node.ELEMENT_NODE
 )
 return result
 
}
function getStruct (structNode) {
 var memberNodes=getNamedChildren(structNode, "member")
 
 var struct = new Object()
 var name
 
 
 var getMemberAndValue = function (x) {
 if (x.nodeName == "name"){
 name = getTextValueOfChild(x) 
 }
 if (x.nodeName == "value"){
 struct[name]=getResultFromValueNode(x)
 }
 }
 
 var eachMember = function (node) {
 visitChildren (
 node,
 getMemberAndValue,
 Node.ELEMENT_NODE
 ) 
 }

 
 eachInNodeList (
 memberNodes,
 eachMember,
 Node.ELEMENT_NODE
 ) 
 return struct

}
function getResultFromValueNode (node) {
 var valueNode = node.firstChild
 var result
 while ("#text"==valueNode.nodeName){
 valueNode = valueNode.nextSibling
 }
 switch (valueNode.nodeName) {
 case "string":
 result = new String(getTextValueOfChild(valueNode))
 break;
 case "i4":
 case "int":
 case "double":
 result= new Number(getTextValueOfChild(valueNode))
 break;
 case "boolean":
 tmp = getTextValueOfChild(valueNode)
 result= tmp=="1"?true:false
 break
 case "dateTime.iso8601":
 result = getDateFromChild(valueNode)
 break
 case "array":
 result = getArray(valueNode)
 break
 case "struct":
 result = getStruct(valueNode)
 break;
 default:
 throw "type not handled: "+valueNode.nodeName
 
 }
 return result
}







function getHandler (rpc, lambda) {
 if (lambda == null) return null
 return function (request) {
 lambda(rpc.handleAnswer(request.responseXML)) 
 } 
}

function XmlRpc (url) {
 this.url=url
 

 this.addArgument=function(arg) { 
 return makeTag ("param", makeTag("value",encodeXmlRpc(arg)))
 }

 this.onerror = null;
 
 this.handleFault=function(fault) {
 value = getNamedChild(fault, "value")
 struct = getNamedChild(value, "struct")
 throw getStruct(struct)  
 
 }
 
 this.handleAnswer=function(xml) {
 if (xml == null) throw "ERROR: communication with the server failed"
 var node = xml.getElementsByTagName("fault")
 if (node.length!=0){this.handleFault(node[0])}
 
 var nodeList = xml.getElementsByTagName("param")
 var valueArr = []
 func = function (node){
 visitChildren(
 node,
 function (y){if (y.nodeName=="value") valueArr.push(y)},
 Node.ELEMENT_NODE
 ) 
 }
 eachInNodeList (nodeList,
 func,
 Node.ELEMENT_NODE
 );
 var result = []

 eachInNodeList (valueArr,
 function (a) {
 result.push(getResultFromValueNode(a))
 },
 Node.ELEMENT_NODE
 )
 return result.length==1 ? result[0] : result

 
 }


 this.call=function () {
 if (arguments.length == 0)
 return null;  
 var request = '<?xml version="1.0"?>'
 request += "<methodCall>" 
 request += makeTag("methodName", arguments[0])
 var params=""
 var callback = null 
 for (var i=1; i!=arguments.length; ++i){

 if (isA(arguments[i],Function)) {
 callback = arguments[i]
 break
 }
 params+=this.addArgument (arguments[i]) 
 }
 if (params != "")
 request += makeTag("params", params)
 request += "</methodCall>"

 var req = new Request(this.url, request, getHandler(this, callback))
 if (callback && this.onerror) {
 req.onnetworkerror = this.onerror
 }

 req.process()
 
 if (callback==null) {
 return this.handleAnswer(req.responseXML)
 }
 
 
 
 }

}


function addFunction (obj, name, javascriptName) {
 obj[javascriptName]=function () {
 retVal = ""
 str = "retVal = this.call(\""+name+"\""
 for (var i=0; i!=arguments.length; ++i){
 str += ", arguments["+i+"]" 
 }
 str+=")"
 eval(str)
 return retVal
 }
}
XmlRpc.getObject = function (url, functionNameArr) {
 obj = new XmlRpc(url)
 functionNameArr = functionNameArr ? functionNameArr : obj.call("system.listMethods")
 for (var i=0; i!= functionNameArr.length; ++i) {
 functionName = functionNameArr[i]
 addFunction(obj, functionName, functionName.replace(/\./,"_"))
 }
 return obj
}

