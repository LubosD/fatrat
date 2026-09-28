export class XmlRpcBinary {
  private constructor(readonly base64: string) {}

  static fromBase64(base64: string): XmlRpcBinary {
    return new XmlRpcBinary(base64);
  }

  static fromFile(file: File): Promise<XmlRpcBinary> {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => {
        const result = reader.result as string;
        // result is a data: URL ("data:<mime>;base64,<data>") - strip the prefix.
        const comma = result.indexOf(',');
        resolve(new XmlRpcBinary(comma >= 0 ? result.slice(comma + 1) : result));
      };
      reader.onerror = () => reject(reader.error);
      reader.readAsDataURL(file);
    });
  }
}

export type XmlRpcValue =
  | string
  | number
  | boolean
  | Date
  | XmlRpcBinary
  | XmlRpcValue[]
  | { [key: string]: XmlRpcValue };

export class XmlRpcFault extends Error {
  constructor(
    readonly faultCode: number,
    readonly faultString: string,
  ) {
    super(`XML-RPC fault ${faultCode}: ${faultString}`);
    this.name = 'XmlRpcFault';
  }
}
