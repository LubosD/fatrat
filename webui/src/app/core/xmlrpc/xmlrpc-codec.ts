import { XmlRpcBinary, XmlRpcFault, XmlRpcValue } from './xmlrpc-value';

function escapeXml(value: string): string {
  return value.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function pad2(n: number): string {
  return n < 10 ? '0' + n : String(n);
}

function encodeIso8601(date: Date): string {
  return (
    `${date.getFullYear()}${pad2(date.getMonth() + 1)}${pad2(date.getDate())}T` +
    `${date.getHours()}:${date.getMinutes()}:${date.getSeconds()}`
  );
}

export function encodeValue(value: XmlRpcValue): string {
  if (value instanceof XmlRpcBinary) {
    return `<base64>${value.base64}</base64>`;
  }
  if (typeof value === 'string') {
    return `<string>${escapeXml(value)}</string>`;
  }
  if (typeof value === 'boolean') {
    return `<boolean>${value ? '1' : '0'}</boolean>`;
  }
  if (value instanceof Date) {
    return `<dateTime.iso8601>${encodeIso8601(value)}</dateTime.iso8601>`;
  }
  if (Array.isArray(value)) {
    const items = value.map((v) => `<value>${encodeValue(v)}</value>`).join('');
    return `<array><data>${items}</data></array>`;
  }
  if (typeof value === 'number') {
    return Number.isInteger(value) ? `<int>${value}</int>` : `<double>${value}</double>`;
  }
  // Plain object -> struct.
  const members = Object.entries(value)
    .map(([key, v]) => `<member><name>${escapeXml(key)}</name><value>${encodeValue(v)}</value></member>`)
    .join('');
  return `<struct>${members}</struct>`;
}

export function encodeMethodCall(method: string, params: XmlRpcValue[]): string {
  const paramsXml = params.map((p) => `<param><value>${encodeValue(p)}</value></param>`).join('');
  const paramsBlock = paramsXml ? `<params>${paramsXml}</params>` : '';
  return `<?xml version="1.0"?><methodCall><methodName>${escapeXml(method)}</methodName>${paramsBlock}</methodCall>`;
}

function findChild(node: Element, name: string): Element | undefined {
  for (const child of Array.from(node.children)) {
    if (child.nodeName === name) return child;
  }
  return undefined;
}

function findChildren(node: Element, name: string): Element[] {
  return Array.from(node.children).filter((c) => c.nodeName === name);
}

function textOf(node: Element): string {
  return (node.textContent ?? '').trim();
}

function decodeIso8601(text: string): Date {
  const match = /^(\d{4})(\d{2})(\d{2})T(\d{1,2}):(\d{1,2}):(\d{1,2})$/.exec(text);
  if (!match) {
    throw new Error(`Invalid dateTime.iso8601 value: ${text}`);
  }
  const [, year, month, day, hour, minute, second] = match;
  return new Date(Number(year), Number(month) - 1, Number(day), Number(hour), Number(minute), Number(second));
}

function decodeArray(arrayNode: Element): XmlRpcValue[] {
  const dataNode = findChild(arrayNode, 'data');
  if (!dataNode) return [];
  return findChildren(dataNode, 'value').map(decodeValueNode);
}

function decodeStruct(structNode: Element): { [key: string]: XmlRpcValue } {
  const result: { [key: string]: XmlRpcValue } = {};
  for (const memberNode of findChildren(structNode, 'member')) {
    const nameNode = findChild(memberNode, 'name');
    const valueNode = findChild(memberNode, 'value');
    if (!nameNode || !valueNode) {
      throw new Error('XML-RPC struct member is missing <name> or <value>');
    }
    result[textOf(nameNode)] = decodeValueNode(valueNode);
  }
  return result;
}

function decodeValueNode(valueNode: Element): XmlRpcValue {
  const typeNode = valueNode.children[0] as Element | undefined;
  if (!typeNode) {
    // No type element means a bare string per XML-RPC convention.
    return textOf(valueNode);
  }
  switch (typeNode.nodeName) {
    case 'string':
      return textOf(typeNode);
    case 'i4':
    case 'int':
    case 'double':
    case 'i8':
    case 'ex:i8':
      return Number(textOf(typeNode));
    case 'boolean':
      return textOf(typeNode) === '1';
    case 'dateTime.iso8601':
      return decodeIso8601(textOf(typeNode));
    case 'array':
      return decodeArray(typeNode);
    case 'struct':
      return decodeStruct(typeNode);
    case 'base64':
      return XmlRpcBinary.fromBase64(textOf(typeNode));
    default:
      throw new Error(`Unsupported XML-RPC type: ${typeNode.nodeName}`);
  }
}

export function decodeMethodResponse(xmlText: string): XmlRpcValue {
  const doc = new DOMParser().parseFromString(xmlText, 'text/xml');
  const parserErrors = doc.getElementsByTagName('parsererror');
  if (parserErrors.length > 0) {
    throw new Error(`Failed to parse XML-RPC response: ${parserErrors[0].textContent}`);
  }

  const root = doc.documentElement;
  if (!root || root.nodeName !== 'methodResponse') {
    throw new Error('XML-RPC response is missing a <methodResponse> root element');
  }

  const faultNodes = doc.getElementsByTagName('fault');
  if (faultNodes.length > 0) {
    const valueNode = findChild(faultNodes[0], 'value');
    if (!valueNode) {
      throw new Error('XML-RPC fault element is missing its value');
    }
    const fault = decodeValueNode(valueNode) as { faultCode?: unknown; faultString?: unknown };
    if (typeof fault.faultCode !== 'number' || typeof fault.faultString !== 'string') {
      throw new Error('XML-RPC fault is missing faultCode or faultString');
    }
    throw new XmlRpcFault(fault.faultCode, fault.faultString);
  }

  const paramNodes = Array.from(doc.getElementsByTagName('param'));
  const values = paramNodes
    .map((paramNode) => findChild(paramNode, 'value'))
    .filter((v): v is Element => v !== undefined)
    .map(decodeValueNode);

  if (values.length === 0) return null as unknown as XmlRpcValue;
  return values.length === 1 ? values[0] : values;
}
