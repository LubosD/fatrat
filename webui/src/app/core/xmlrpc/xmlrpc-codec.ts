import { XmlRpcBinary, XmlRpcValue } from './xmlrpc-value';

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
