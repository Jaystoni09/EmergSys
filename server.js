const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');

const port = Number(process.env.PORT || 8000);
const root = __dirname;
const mimeTypes = { '.html': 'text/html; charset=utf-8', '.css': 'text/css; charset=utf-8', '.js': 'text/javascript; charset=utf-8' };
const state = { adminId: 'admin', adminPassword: 'admin123', active: false, severity: 'WAITING', room: '--', symptoms: 'No alert received', receivedAt: '--', response: 'PENDING', history: [] };
let lastAlertKey = '';
let lastAlertAt = 0;

function send(response, status, type, body) {
  response.writeHead(status, { 'Content-Type': type, 'Access-Control-Allow-Origin': '*', 'Cache-Control': 'no-store' });
  response.end(body);
}

function json(response, status, value) {
  send(response, status, 'application/json; charset=utf-8', JSON.stringify(value));
}

function readBody(request) {
  return new Promise((resolve, reject) => {
    let body = '';
    request.setEncoding('utf8');
    request.on('data', (chunk) => { body += chunk; });
    request.on('end', () => resolve(body ? JSON.parse(body) : {}));
    request.on('error', reject);
  });
}

function staticFile(response, requestPath) {
  const relativePath = requestPath === '/' ? 'index.html' : requestPath.slice(1);
  const filePath = path.join(root, relativePath);
  if (!filePath.startsWith(root) || !fs.existsSync(filePath)) return send(response, 404, 'text/plain; charset=utf-8', 'Not found');
  const extension = path.extname(filePath).toLowerCase();
  send(response, 200, mimeTypes[extension] || 'application/octet-stream', fs.readFileSync(filePath));
}

const server = http.createServer(async (request, response) => {
  const requestUrl = new URL(request.url, `http://${request.headers.host || 'localhost'}`);
  const requestPath = requestUrl.pathname;

  if (request.method === 'OPTIONS') {
    response.writeHead(204, { 'Access-Control-Allow-Origin': '*', 'Access-Control-Allow-Methods': 'GET, POST, OPTIONS', 'Access-Control-Allow-Headers': 'Content-Type' });
    return response.end();
  }
  if (request.method === 'POST' && requestPath === '/api/login') {
    try {
      const payload = await readBody(request);
      const valid = payload.adminId === state.adminId && payload.password === state.adminPassword;
      return json(response, valid ? 200 : 401, valid ? { ok: true } : { ok: false, error: 'Invalid admin ID or password.' });
    } catch (error) { return json(response, 400, { error: error.message }); }
  }
  if (request.method === 'POST' && requestPath === '/api/password/change') {
    try {
      const payload = await readBody(request);
      const valid = payload.adminId === state.adminId && payload.currentPassword === state.adminPassword;
      if (!valid) return json(response, 401, { ok: false, error: 'Admin ID or current password is incorrect.' });
      state.adminPassword = String(payload.newPassword);
      return json(response, 200, { ok: true });
    } catch (error) { return json(response, 400, { error: error.message }); }
  }
  if (request.method === 'GET' && requestPath === '/health') return json(response, 200, { ok: true });
  if (request.method === 'GET' && requestPath === '/api/status') {
    return json(response, 200, { active: state.active, severity: state.severity, room: state.room, symptoms: state.symptoms, receivedAt: state.receivedAt, response: state.response, history: state.history });
  }

  try {
    if (request.method === 'POST' && requestPath === '/api/alert') {
      const payload = await readBody(request);
      state.active = Boolean(payload.active);
      state.severity = String(payload.severity || 'MILD');
      state.room = String(payload.room || '--');
      state.symptoms = String(payload.symptoms || '');
      state.receivedAt = new Date().toLocaleTimeString('en-GB');
      state.response = 'PENDING';
      const alertKey = `${state.room}|${state.severity}|${state.symptoms}`;
      if (alertKey !== lastAlertKey || Date.now() - lastAlertAt >= 5000) {
        state.history.unshift({ receivedAt: state.receivedAt, room: state.room, severity: state.severity, symptoms: state.symptoms, response: 'PENDING' });
        state.history = state.history.slice(0, 20);
        lastAlertKey = alertKey;
        lastAlertAt = Date.now();
      }
      return json(response, 200, { ok: true });
    }
    if (request.method === 'POST' && requestPath === '/api/response') {
      const payload = await readBody(request);
      const accepted = state.response === 'PENDING';
      if (accepted) {
        state.response = String(payload.decision || 'DENIED');
        if (state.history[0]) state.history[0].response = state.response;
      }
      return json(response, 200, { ok: accepted, response: state.response });
    }
    if (request.method === 'POST' && requestPath === '/api/history/clear') {
      state.history = [];
      return json(response, 200, { ok: true });
    }
    if (request.method === 'GET') return staticFile(response, requestPath);
    return json(response, 404, { error: 'Not found' });
  } catch (error) {
    return json(response, 400, { error: error.message });
  }
});

server.listen(port, '0.0.0.0', () => console.log(`Emergency receiver listening on port ${port}`));
