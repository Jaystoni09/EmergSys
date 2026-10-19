const state = { emergencyActive: false, lastAlertKey: '', history: [] };
const ADMIN_ID = 'admin';
const DEFAULT_ADMIN_PASSWORD = 'admin123';
const loginScreen = document.getElementById('loginScreen');
const dashboard = document.getElementById('dashboard');
const loginForm = document.getElementById('loginForm');
const loginError = document.getElementById('loginError');
const loginButton = loginForm.querySelector('button[type="submit"]');
const passwordDialog = document.getElementById('passwordDialog');
const passwordForm = document.getElementById('passwordForm');
const passwordError = document.getElementById('passwordError');

function getAdminPassword() {
  return localStorage.getItem('emergencyAdminPassword') || DEFAULT_ADMIN_PASSWORD;
}

const statusPanel = document.getElementById('statusPanel');
const statusLabel = document.getElementById('statusLabel');
const roomNumber = document.getElementById('roomNumber');
const severity = document.getElementById('severity');
const symptoms = document.getElementById('symptoms');
const statusDescription = document.getElementById('statusDescription');
const receivedAt = document.getElementById('receivedAt');
const historyList = document.getElementById('historyList');
const historyCount = document.getElementById('historyCount');
const alertDialog = document.getElementById('alertDialog');
const dialogRoom = document.getElementById('dialogRoom');
const dialogSeverity = document.getElementById('dialogSeverity');
const dialogSymptoms = document.getElementById('dialogSymptoms');
const toast = document.getElementById('toast');
const deleteDialog = document.getElementById('deleteDialog');
let alertAudioContext;

function unlockAlertAudio() {
  if (!alertAudioContext) {
    const AudioContextClass = window.AudioContext || window.webkitAudioContext;
    if (AudioContextClass) alertAudioContext = new AudioContextClass();
  }
  if (alertAudioContext && alertAudioContext.state === 'suspended') alertAudioContext.resume();
}

function playAlertSound() {
  unlockAlertAudio();
  if (!alertAudioContext) return;
  const startTime = alertAudioContext.currentTime;
  [0, 0.45, 0.9].forEach((offset) => {
    const oscillator = alertAudioContext.createOscillator();
    const gain = alertAudioContext.createGain();
    oscillator.frequency.value = 740;
    oscillator.type = 'square';
    gain.gain.setValueAtTime(0.0001, startTime + offset);
    gain.gain.exponentialRampToValueAtTime(0.18, startTime + offset + 0.02);
    gain.gain.exponentialRampToValueAtTime(0.0001, startTime + offset + 0.22);
    oscillator.connect(gain).connect(alertAudioContext.destination);
    oscillator.start(startTime + offset);
    oscillator.stop(startTime + offset + 0.24);
  });
}

function showToast(message) {
  toast.textContent = message;
  toast.classList.add('show');
  window.clearTimeout(showToast.timeout);
  showToast.timeout = window.setTimeout(() => toast.classList.remove('show'), 3200);
}

function renderHistory(history) {
  state.history = history;
  historyCount.textContent = `${history.length} alert${history.length === 1 ? '' : 's'}`;
  if (!history.length) {
    historyList.innerHTML = '<p class="history-empty">No alerts received yet.</p>';
    return;
  }

  historyList.innerHTML = history.map((entry) => `
    <article class="history-entry ${['HEAVY', 'MEDICAL', 'MEDICAL EMERGENCIES'].includes(String(entry.severity).toUpperCase()) ? 'medical-entry' : ''}">
      <span class="history-severity">${entry.severity}</span>
      <div class="history-copy"><strong>Room ${entry.room}</strong><span>${entry.symptoms}</span></div>
      <div class="history-meta"><time>${entry.receivedAt}</time><span class="history-response ${entry.response === 'CONFIRMED' ? 'confirmed' : entry.response === 'DENIED' ? 'denied' : ''}">${entry.response || 'PENDING'}</span></div>
    </article>
  `).join('');
}

function receiveAlert(alert) {
  const alertSeverity = String(alert.severity || 'MILD').toUpperCase();
  const alertRoom = alert.room || '--';
  const alertSymptoms = alert.symptoms || (alertSeverity === 'HEAVY'
    ? 'Chest pain, shortness of breath, severe pain'
    : 'Headache, nausea, mild pain');
  const alertKey = `${alert.receivedAt || ''}|${alertSeverity}|${alertRoom}|${alertSymptoms}`;

  state.emergencyActive = true;
  statusPanel.classList.add('active');
  statusLabel.textContent = `${alertSeverity} ALERT`;
  roomNumber.textContent = alertRoom;
  severity.textContent = alertSeverity;
  symptoms.textContent = alertSymptoms;
  statusDescription.textContent = `Respond to Room ${alertRoom}.`;
  receivedAt.textContent = alert.receivedAt || new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  renderHistory(alert.history || []);

  if (alert.response === 'CONFIRMED' || alert.response === 'DENIED') {
    applyRemoteResponse(alert.response);
  } else if (alertKey !== state.lastAlertKey) {
    state.lastAlertKey = alertKey;
    playAlertSound();
    openAlertDialog(alertSeverity, alertRoom, alertSymptoms);
  }
}

function openAlertDialog(alertSeverity, alertRoom, alertSymptoms) {
  dialogSeverity.textContent = alertSeverity;
  dialogRoom.textContent = alertRoom;
  dialogSymptoms.textContent = alertSymptoms;
  alertDialog.hidden = false;
}

function applyRemoteResponse(response) {
  if (response !== 'CONFIRMED' && response !== 'DENIED') return;
  alertDialog.hidden = true;
  statusDescription.textContent = `Alert ${response.toLowerCase()} by another receiver.`;
  showToast(`Alert ${response.toLowerCase()} on another device.`);
}

async function respondToAlert(decision) {
  try {
    const response = await fetch('/api/response', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ decision })
    });
    if (!response.ok) throw new Error('Response was not accepted');
    const result = await response.json();
    alertDialog.hidden = true;
    const winningDecision = result.response || decision;
    statusDescription.textContent = result.ok
      ? `Alert ${winningDecision.toLowerCase()} by this receiver.`
      : `Alert ${winningDecision.toLowerCase()} by another receiver.`;
    showToast(result.ok ? `Alert ${winningDecision.toLowerCase()}.` : `Another device already ${winningDecision.toLowerCase()} this alert.`);
  } catch (error) {
    showToast('Unable to send the operator decision.');
  }
}

async function clearHistory() {
  deleteDialog.hidden = false;
}

async function confirmClearHistory() {
  try {
    const response = await fetch('/api/history/clear', { method: 'POST' });
    if (!response.ok) throw new Error('History could not be cleared');
    renderHistory([]);
    deleteDialog.hidden = true;
    showToast('Alert history cleared.');
  } catch (error) {
    showToast('Unable to clear alert history.');
  }
}

function csvValue(value) {
  return `"${String(value ?? '').replaceAll('"', '""')}"`;
}

function saveHistoryCsv() {
  const header = ['Received at', 'Room', 'Severity', 'Symptoms', 'Response'];
  const rows = state.history.map((entry) => [entry.receivedAt, entry.room, entry.severity, entry.symptoms, entry.response || 'PENDING']);
  const csv = [header, ...rows].map((row) => row.map(csvValue).join(',')).join('\r\n');
  const download = document.createElement('a');
  download.href = URL.createObjectURL(new Blob([csv], { type: 'text/csv;charset=utf-8' }));
  download.download = `emergency-alert-history-${new Date().toISOString().slice(0, 10)}.csv`;
  download.click();
  URL.revokeObjectURL(download.href);
  showToast('Alert history saved as CSV.');
}

function clearAlert() {
  state.emergencyActive = false;
  state.lastAlertKey = '';
  statusPanel.classList.remove('active');
  statusLabel.textContent = 'LISTENING';
  roomNumber.textContent = '--';
  severity.textContent = 'WAITING';
  symptoms.textContent = 'No alert received';
  statusDescription.textContent = 'Ready for an ESP32 alert.';
  receivedAt.textContent = '--';
  alertDialog.hidden = true;
}

function startDashboard() {
  loginScreen.hidden = true;
  dashboard.hidden = false;
  pollReceiver();
  window.setInterval(pollReceiver, 2000);
}

async function pollReceiver() {
  try {
    const response = await fetch('/api/status');
    const alert = await response.json();
    renderHistory(alert.history || []);
    if (alert.active) receiveAlert(alert);
  } catch (error) {
    statusLabel.textContent = 'RECEIVER OFFLINE';
  }
}

loginForm.addEventListener('submit', (event) => {
  event.preventDefault();
  unlockAlertAudio();
  const formData = new FormData(loginForm);
  if (formData.get('adminId') === ADMIN_ID && formData.get('adminPassword') === getAdminPassword()) {
    sessionStorage.setItem('emergencyAdminAuthenticated', 'true');
    loginError.textContent = '';
    loginButton.disabled = true;
    loginButton.textContent = 'SIGNING IN...';
    window.setTimeout(startDashboard, 700);
  } else {
    loginError.textContent = 'Invalid admin ID or password.';
  }
});

document.getElementById('openPasswordChange').addEventListener('click', () => {
  passwordError.textContent = '';
  passwordForm.reset();
  passwordDialog.hidden = false;
});
document.getElementById('cancelPassword').addEventListener('click', () => { passwordDialog.hidden = true; });
passwordForm.addEventListener('submit', (event) => {
  event.preventDefault();
  const currentPassword = document.getElementById('currentPassword').value;
  const newPassword = document.getElementById('newPassword').value;
  const confirmPassword = document.getElementById('confirmPassword').value;
  if (currentPassword !== getAdminPassword()) { passwordError.textContent = 'Current password is incorrect.'; return; }
  if (newPassword.length < 6) { passwordError.textContent = 'New password must be at least 6 characters.'; return; }
  if (newPassword !== confirmPassword) { passwordError.textContent = 'New passwords do not match.'; return; }
  localStorage.setItem('emergencyAdminPassword', newPassword);
  passwordDialog.hidden = true;
  loginError.textContent = 'Password changed successfully.';
  loginError.style.color = 'var(--lime)';
});

document.addEventListener('pointerdown', unlockAlertAudio, { once: true });

document.getElementById('confirmAlert').addEventListener('click', () => respondToAlert('CONFIRMED'));
document.getElementById('denyAlert').addEventListener('click', () => respondToAlert('DENIED'));
document.getElementById('clearHistory').addEventListener('click', clearHistory);
document.getElementById('cancelDelete').addEventListener('click', () => { deleteDialog.hidden = true; });
document.getElementById('confirmDelete').addEventListener('click', confirmClearHistory);
document.getElementById('saveHistory').addEventListener('click', saveHistoryCsv);
document.getElementById('signOut').addEventListener('click', () => {
  sessionStorage.removeItem('emergencyAdminAuthenticated');
  window.location.reload();
});
window.receiveAlert = receiveAlert;
window.clearAlert = clearAlert;

if (sessionStorage.getItem('emergencyAdminAuthenticated') === 'true') {
  startDashboard();
}
