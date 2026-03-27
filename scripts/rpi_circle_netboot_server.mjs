#!/usr/bin/env node

import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import dgram from "node:dgram";
import { spawn, spawnSync } from "node:child_process";

const DHCP_MAGIC = 0x63825363;
const DHCP_OPTION_END = 255;
const DHCP_OPTION_PAD = 0;
const DHCPDISCOVER = 1;
const DHCPOFFER = 2;
const DHCPREQUEST = 3;
const DHCPACK = 5;
const DHCP_SERVER_PORT = 67;
const DHCP_CLIENT_PORT = 68;
const TFTP_PORT = 69;
const TFTP_BLOCK_SIZE = 512;

function parseArgs(argv) {
  const options = {
    bootDir: path.resolve(".pio/build/rpi_circle_dev/circle/boot"),
    serverIp: "",
    clientIp: "10.42.0.9",
    subnetMask: "255.255.255.0",
    router: "",
    dns: "",
    bootfile: "",
    hostname: "fluidtouch-rpi4",
    iface: "",
    leaseSeconds: 86400,
    dhcpPort: DHCP_SERVER_PORT,
    tftpPort: TFTP_PORT,
    nfsEnabled: true,
    nfsDir: "",
    nfsExportTarget: "",
    check: false,
    help: false,
  };

  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    const next = argv[index + 1];
    if (arg === "--help" || arg === "-h") {
      options.help = true;
    } else if (arg === "--check") {
      options.check = true;
    } else if (arg === "--boot-dir" && next) {
      options.bootDir = path.resolve(next);
      index += 1;
    } else if (arg === "--server-ip" && next) {
      options.serverIp = next;
      index += 1;
    } else if (arg === "--client-ip" && next) {
      options.clientIp = next;
      index += 1;
    } else if (arg === "--subnet-mask" && next) {
      options.subnetMask = next;
      index += 1;
    } else if (arg === "--router" && next) {
      options.router = next;
      index += 1;
    } else if (arg === "--dns" && next) {
      options.dns = next;
      index += 1;
    } else if (arg === "--bootfile" && next) {
      options.bootfile = next;
      index += 1;
    } else if (arg === "--hostname" && next) {
      options.hostname = next;
      index += 1;
    } else if (arg === "--iface" && next) {
      options.iface = next;
      index += 1;
    } else if (arg === "--dhcp-port" && next) {
      options.dhcpPort = Number(next);
      index += 1;
    } else if (arg === "--tftp-port" && next) {
      options.tftpPort = Number(next);
      index += 1;
    } else if (arg === "--lease-seconds" && next) {
      options.leaseSeconds = Number(next);
      index += 1;
    } else if (arg === "--nfs-dir" && next) {
      options.nfsDir = path.resolve(next);
      index += 1;
    } else if (arg === "--nfs-export-target" && next) {
      options.nfsExportTarget = next;
      index += 1;
    } else if (arg === "--no-nfs") {
      options.nfsEnabled = false;
    } else {
      throw new Error(`Unknown argument: ${arg}`);
    }
  }

  return options;
}

function usage() {
  console.log(`Usage:
  node scripts/rpi_circle_netboot_server.mjs [options]

Options:
  --boot-dir <dir>       Boot payload directory (default: .pio/build/rpi_circle_dev/circle/boot)
  --server-ip <ip>       IP address served to the Pi as next-server / gateway
  --client-ip <ip>       Fixed Pi lease (default: 10.42.0.9)
  --subnet-mask <mask>   DHCP subnet mask (default: 255.255.255.0)
  --router <ip>          DHCP router address (default: server IP)
  --dns <ip>             DHCP DNS server (default: server IP)
  --iface <name>         Host interface name used to infer server IPv4
  --bootfile <name>      Initial TFTP boot file (default: netboot.json bootfile or start4.elf)
  --hostname <name>      Hostname announced in logs
  --dhcp-port <port>     DHCP listen port (default: 67)
  --tftp-port <port>     TFTP listen port (default: 69)
  --lease-seconds <n>    DHCP lease time (default: 86400)
  --nfs-dir <dir>        Export directory for NFS (default: boot dir)
  --nfs-export-target    Host or CIDR allowed to mount NFS (default: client IP)
  --no-nfs               Disable NFS export and DHCP root-path advertisement
  --check                Validate files and print resolved config without serving
  --help                 Show this help

Notes:
  - Run on an isolated Pi 4 Ethernet link.
  - Ports 67 and 69 usually require root or CAP_NET_BIND_SERVICE/CAP_NET_ADMIN.
  - NFS mode expects host tools such as exportfs, rpc.nfsd, and rpc.mountd.
  - The Pi 4 EEPROM must already be configured for network boot.`);
}

function parseManifest(bootDir) {
  const manifestPath = path.join(bootDir, "netboot.json");
  if (!fs.existsSync(manifestPath)) {
    return {};
  }
  return JSON.parse(fs.readFileSync(manifestPath, "utf8"));
}

function getInterfaceIpv4(ifaceName) {
  const interfaces = os.networkInterfaces();
  if (ifaceName) {
    const entries = interfaces[ifaceName] || [];
    const ipv4 = entries.find((entry) => entry.family === "IPv4" && !entry.internal);
    return ipv4 ? ipv4.address : "";
  }

  for (const entries of Object.values(interfaces)) {
    for (const entry of entries || []) {
      if (entry.family === "IPv4" && !entry.internal) {
        return entry.address;
      }
    }
  }
  return "";
}

function ipv4ToBuffer(address) {
  const parts = address.split(".").map((part) => Number(part));
  if (parts.length !== 4 || parts.some((part) => !Number.isInteger(part) || part < 0 || part > 255)) {
    throw new Error(`Invalid IPv4 address: ${address}`);
  }
  return Buffer.from(parts);
}

function makeOption(code, value) {
  return Buffer.concat([Buffer.from([code, value.length]), value]);
}

function readDhcpOptions(buffer) {
  const options = new Map();
  let offset = 240;
  while (offset < buffer.length) {
    const code = buffer[offset];
    offset += 1;
    if (code === DHCP_OPTION_END) {
      break;
    }
    if (code === DHCP_OPTION_PAD) {
      continue;
    }
    if (offset >= buffer.length) {
      break;
    }
    const length = buffer[offset];
    offset += 1;
    const value = buffer.subarray(offset, offset + length);
    offset += length;
    options.set(code, value);
  }
  return options;
}

function readZeroTerminatedStrings(buffer, startOffset) {
  const strings = [];
  let offset = startOffset;
  while (offset < buffer.length) {
    const end = buffer.indexOf(0, offset);
    if (end === -1) {
      break;
    }
    if (end === offset) {
      break;
    }
    strings.push(buffer.toString("utf8", offset, end));
    offset = end + 1;
  }
  return strings;
}

function validateBootDir(bootDir, bootfile) {
  if (!fs.existsSync(bootDir) || !fs.statSync(bootDir).isDirectory()) {
    throw new Error(`Boot directory not found: ${bootDir}`);
  }

  const required = [bootfile, "config.txt", "cmdline.txt", "start4.elf", "fixup4.dat", "bcm2711-rpi-4-b.dtb"];
  for (const file of required) {
    const filePath = path.join(bootDir, file);
    if (!fs.existsSync(filePath)) {
      throw new Error(`Required boot file missing: ${filePath}`);
    }
  }
}

function formatBytes(length) {
  return `${length} bytes`;
}

function findCommand(name) {
  const candidates = [name, `/usr/sbin/${name}`, `/sbin/${name}`, `/usr/bin/${name}`, `/bin/${name}`];
  for (const candidate of candidates) {
    if (candidate.includes("/")) {
      if (fs.existsSync(candidate)) {
        return candidate;
      }
      continue;
    }

    const result = spawnSync("sh", ["-lc", `command -v ${candidate}`], { encoding: "utf8" });
    if (result.status === 0) {
      return result.stdout.trim();
    }
  }
  return "";
}

function resolveFile(bootDir, requestPath) {
  const relative = requestPath.replace(/^\/+/, "");
  const normalized = path.posix.normalize(relative);
  if (normalized.startsWith("../") || normalized === "..") {
    return "";
  }
  return path.join(bootDir, normalized);
}

function validateNfsConfig(config) {
  if (!config.nfsEnabled) {
    return;
  }

  if (!fs.existsSync(config.nfsDir) || !fs.statSync(config.nfsDir).isDirectory()) {
    throw new Error(`NFS export directory not found: ${config.nfsDir}`);
  }

  const required = ["exportfs", "rpc.nfsd", "rpc.mountd"];
  for (const name of required) {
    const command = findCommand(name);
    if (!command) {
      throw new Error(`Missing required NFS tool '${name}'. Install nfs-kernel-server or run with --no-nfs.`);
    }
  }
}

function spawnChecked(command, args, options = {}) {
  const result = spawnSync(command, args, { stdio: "pipe", encoding: "utf8", ...options });
  if (result.status !== 0) {
    const stderr = result.stderr?.trim();
    const stdout = result.stdout?.trim();
    const detail = stderr || stdout || `exit ${result.status}`;
    throw new Error(`${command} ${args.join(" ")} failed: ${detail}`);
  }
  return result;
}

function startNfsServices(config) {
  if (!config.nfsEnabled) {
    return null;
  }

  const exportfs = findCommand("exportfs");
  const rpcNfsd = findCommand("rpc.nfsd");
  const rpcMountd = findCommand("rpc.mountd");
  const exportTarget = config.nfsExportTarget || config.clientIp;
  const exportSpec = `${exportTarget}:${config.nfsDir}`;
  const exportOptions = "ro,async,no_subtree_check,no_root_squash,insecure";

  console.log(`NFS export: ${exportSpec} (${exportOptions})`);
  spawnChecked(exportfs, ["-i", "-o", exportOptions, exportSpec]);
  spawnChecked(rpcNfsd, ["8"]);

  const mountd = spawn(rpcMountd, ["--foreground", "--no-nfs-version", "2", "--nfs-version", "3"], {
    stdio: ["ignore", "pipe", "pipe"],
  });
  mountd.stdout.on("data", (chunk) => {
    const text = chunk.toString("utf8").trim();
    if (text) {
      console.log(`mountd: ${text}`);
    }
  });
  mountd.stderr.on("data", (chunk) => {
    const text = chunk.toString("utf8").trim();
    if (text) {
      console.error(`mountd: ${text}`);
    }
  });
  mountd.on("exit", (code, signal) => {
    console.log(`mountd exited (${signal || code || 0})`);
  });

  return {
    mountd,
    stop() {
      try {
        spawnChecked(exportfs, ["-u", exportSpec]);
      } catch (error) {
        console.error(error instanceof Error ? error.message : String(error));
      }
      try {
        mountd.kill("SIGTERM");
      } catch {
        // Ignore shutdown races.
      }
    },
  };
}

function createDhcpServer(config) {
  const socket = dgram.createSocket("udp4");

  socket.on("error", (error) => {
    console.error(`DHCP error: ${error.message}`);
  });

  socket.on("message", (message, remote) => {
    if (message.length < 240 || message.readUInt32BE(236) !== DHCP_MAGIC) {
      return;
    }

    const options = readDhcpOptions(message);
    const messageType = options.get(53)?.[0];
    if (messageType !== DHCPDISCOVER && messageType !== DHCPREQUEST) {
      return;
    }

    const xid = message.subarray(4, 8);
    const chaddr = message.subarray(28, 34);
    const responseType = messageType === DHCPDISCOVER ? DHCPOFFER : DHCPACK;
    const response = Buffer.alloc(300, 0);

    response[0] = 2;
    response[1] = 1;
    response[2] = 6;
    response[3] = 0;
    xid.copy(response, 4);
    message.subarray(10, 12).copy(response, 10);
    ipv4ToBuffer(config.clientIp).copy(response, 16);
    ipv4ToBuffer(config.serverIp).copy(response, 20);
    chaddr.copy(response, 28);
    response.write(config.bootfile, 108, 128, "ascii");
    response.writeUInt32BE(DHCP_MAGIC, 236);

    const optionBuffers = [
      makeOption(53, Buffer.from([responseType])),
      makeOption(1, ipv4ToBuffer(config.subnetMask)),
      makeOption(3, ipv4ToBuffer(config.router)),
      makeOption(6, ipv4ToBuffer(config.dns)),
      makeOption(28, ipv4ToBuffer(config.broadcastIp)),
      makeOption(51, Buffer.from([
        (config.leaseSeconds >>> 24) & 0xff,
        (config.leaseSeconds >>> 16) & 0xff,
        (config.leaseSeconds >>> 8) & 0xff,
        config.leaseSeconds & 0xff,
      ])),
      makeOption(54, ipv4ToBuffer(config.serverIp)),
      makeOption(66, Buffer.from(config.serverIp, "ascii")),
      makeOption(67, Buffer.from(config.bootfile, "ascii")),
      ...(config.nfsEnabled ? [makeOption(17, Buffer.from(`${config.serverIp}:${config.nfsDir}`, "ascii"))] : []),
      Buffer.from([DHCP_OPTION_END]),
    ];
    const optionBlock = Buffer.concat(optionBuffers);
    optionBlock.copy(response, 240);

    socket.send(response.subarray(0, 240 + optionBlock.length), DHCP_CLIENT_PORT, "255.255.255.255", (error) => {
      if (error) {
        console.error(`DHCP send error: ${error.message}`);
        return;
      }
      const label = responseType === DHCPOFFER ? "offer" : "ack";
      console.log(`DHCP ${label}: ${config.clientIp} -> ${chaddr.toString("hex").match(/../g).join(":")} (${remote.address})`);
    });
  });

  socket.bind(config.dhcpPort, "0.0.0.0", () => {
    socket.setBroadcast(true);
    console.log(`DHCP listening on udp://0.0.0.0:${config.dhcpPort}`);
  });

  return socket;
}

function sendTftpError(socket, code, message, port, address) {
  const text = Buffer.from(message, "ascii");
  const payload = Buffer.alloc(4 + text.length + 1);
  payload.writeUInt16BE(5, 0);
  payload.writeUInt16BE(code, 2);
  text.copy(payload, 4);
  socket.send(payload, port, address);
}

function createTftpSession(config, filename, remotePort, remoteAddress) {
  const filePath = resolveFile(config.bootDir, filename);
  if (!filePath) {
    console.warn(`TFTP reject: ${filename} -> path escape blocked`);
    return null;
  }

  if (!fs.existsSync(filePath) || !fs.statSync(filePath).isFile()) {
    console.warn(`TFTP miss: ${filename} -> ${filePath}`);
    return null;
  }

  const data = fs.readFileSync(filePath);
  console.log(`TFTP load: ${filename} -> ${filePath} (${formatBytes(data.length)})`);
  const socket = dgram.createSocket("udp4");
  let block = 1;
  let closed = false;

  function close() {
    if (!closed) {
      closed = true;
      socket.close();
    }
  }

  function sendBlock() {
    const start = (block - 1) * TFTP_BLOCK_SIZE;
    const end = Math.min(start + TFTP_BLOCK_SIZE, data.length);
    const chunk = data.subarray(start, end);
    const payload = Buffer.alloc(4 + chunk.length);
    payload.writeUInt16BE(3, 0);
    payload.writeUInt16BE(block, 2);
    chunk.copy(payload, 4);
    socket.send(payload, remotePort, remoteAddress);
  }

  socket.on("error", (error) => {
    console.error(`TFTP session error for ${filename}: ${error.message}`);
    close();
  });

  socket.on("message", (message, peer) => {
    if (peer.port !== remotePort || peer.address !== remoteAddress || message.length < 4) {
      return;
    }
    const opcode = message.readUInt16BE(0);
    const ackBlock = message.readUInt16BE(2);
    if (opcode !== 4 || ackBlock !== block) {
      return;
    }

    const sentLastBlock = block * TFTP_BLOCK_SIZE >= data.length;
    if (sentLastBlock) {
      console.log(`TFTP complete: ${filename} -> ${remoteAddress}:${remotePort}`);
      close();
      return;
    }

    block += 1;
    sendBlock();
  });

  socket.bind(0, () => {
    console.log(`TFTP rrq: ${filename} -> ${remoteAddress}:${remotePort}`);
    sendBlock();
  });

  return socket;
}

function createTftpServer(config) {
  const socket = dgram.createSocket("udp4");

  socket.on("error", (error) => {
    console.error(`TFTP error: ${error.message}`);
  });

  socket.on("message", (message, remote) => {
    if (message.length < 4) {
      return;
    }
    const opcode = message.readUInt16BE(0);
    if (opcode !== 1) {
      sendTftpError(socket, 4, "Only RRQ is supported", remote.port, remote.address);
      return;
    }

    const [filename, mode] = readZeroTerminatedStrings(message, 2);
    if (!filename) {
      sendTftpError(socket, 0, "Missing filename", remote.port, remote.address);
      return;
    }
    if (mode && mode.toLowerCase() !== "octet") {
      sendTftpError(socket, 0, "Only octet mode is supported", remote.port, remote.address);
      return;
    }

    const session = createTftpSession(config, filename, remote.port, remote.address);
    if (!session) {
      console.warn(`TFTP error: ${filename} -> ${remote.address}:${remote.port} (not served)`);
      sendTftpError(socket, 1, "File not found", remote.port, remote.address);
    }
  });

  socket.bind(config.tftpPort, "0.0.0.0", () => {
    console.log(`TFTP listening on udp://0.0.0.0:${config.tftpPort}`);
  });

  return socket;
}

function makeBroadcastIp(clientIp, subnetMask) {
  const client = ipv4ToBuffer(clientIp);
  const mask = ipv4ToBuffer(subnetMask);
  const broadcast = Buffer.alloc(4);
  for (let index = 0; index < 4; index += 1) {
    broadcast[index] = (client[index] & mask[index]) | (~mask[index] & 0xff);
  }
  return Array.from(broadcast).join(".");
}

function main() {
  const options = parseArgs(process.argv.slice(2));
  if (options.help) {
    usage();
    process.exit(0);
  }

  const manifest = parseManifest(options.bootDir);
  const config = {
    ...options,
    serverIp: options.serverIp || getInterfaceIpv4(options.iface),
    router: options.router || options.serverIp || getInterfaceIpv4(options.iface),
    dns: options.dns || options.serverIp || getInterfaceIpv4(options.iface),
    bootfile: options.bootfile || manifest.bootfile || "start4.elf",
  };

  if (!config.serverIp) {
    throw new Error("Unable to determine server IP. Pass --server-ip or --iface.");
  }

  config.router = config.router || config.serverIp;
  config.dns = config.dns || config.serverIp;
  config.nfsDir = options.nfsDir || config.bootDir;
  config.nfsExportTarget = options.nfsExportTarget || config.clientIp;
  config.broadcastIp = makeBroadcastIp(config.clientIp, config.subnetMask);

  validateBootDir(config.bootDir, config.bootfile);
  validateNfsConfig(config);

  console.log(`Netboot boot dir: ${config.bootDir}`);
  console.log(`Netboot server IP: ${config.serverIp}`);
  console.log(`Netboot client IP: ${config.clientIp}`);
  console.log(`Netboot boot file: ${config.bootfile}`);
  if (config.nfsEnabled) {
    console.log(`Netboot NFS export: ${config.nfsDir} -> ${config.nfsExportTarget}`);
  }

  if (config.check) {
    return;
  }

  const nfs = startNfsServices(config);
  const sockets = [createDhcpServer(config), createTftpServer(config)];
  const shutdown = () => {
    for (const socket of sockets) {
      try {
        socket.close();
      } catch {
        // Ignore shutdown races.
      }
    }
    try {
      nfs?.stop();
    } catch {
      // Ignore shutdown races.
    }
    process.exit(0);
  };

  process.on("SIGINT", shutdown);
  process.on("SIGTERM", shutdown);
}

try {
  main();
} catch (error) {
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
}
