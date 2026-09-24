const SESSION_KEY = 'smart_banking_current_account';
const LEGACY_SESSION_KEY = 'northstar_current_account';
const LIVE_API_BASE = 'https://smart-banking-us4t.onrender.com';
const DEFAULT_API_BASE = 'http://localhost:8080';
const API_BASE = window.__SMART_BANKING_API_URL__ || LIVE_API_BASE || (
  (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1')
    ? DEFAULT_API_BASE
    : LIVE_API_BASE
);

function getSessionAccount() {
  const currentAccount = sessionStorage.getItem(SESSION_KEY);
  if (currentAccount) return currentAccount;

  const legacyAccount = sessionStorage.getItem(LEGACY_SESSION_KEY);
  if (legacyAccount) {
    sessionStorage.setItem(SESSION_KEY, legacyAccount);
    sessionStorage.removeItem(LEGACY_SESSION_KEY);
  }
  return legacyAccount;
}

function money(value) {
  return `₹${Number(value).toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}`;
}

function showToast(message) {
  const toast = document.querySelector('#toast');
  if (!toast) return;
  toast.textContent = message;
  toast.classList.add('show');
  window.clearTimeout(showToast.timer);
  showToast.timer = window.setTimeout(() => toast.classList.remove('show'), 2600);
}

async function apiRequest(endpoint, options = {}) {
  const response = await fetch(`${API_BASE}${endpoint}`, {
    mode: 'cors',
    headers: { 'Content-Type': 'application/json' },
    ...options
  });

  const text = await response.text();
  let payload = {};

  try {
    payload = text ? JSON.parse(text) : {};
  } catch (error) {
    payload = { message: text || 'Request failed' };
  }

  if (!response.ok) {
    throw new Error(payload.message || 'Request failed');
  }

  return payload;
}

function renderTransactions(account) {
  const list = document.querySelector('#transactionList');
  if (!list) return;

  const transactions = Array.isArray(account.transactions) ? account.transactions : [];

  list.innerHTML = transactions.map((transaction, index) => {
    const text = String(transaction || 'Transaction');
    const outgoing = text.toLowerCase().includes('withdraw') || text.toLowerCase().includes('transfer') || text.toLowerCase().includes('spent');
    return `
      <div class="transaction">
        <span class="transaction-icon ${outgoing ? 'out' : ''}">${outgoing ? '↗' : '↙'}</span>
        <div class="transaction-copy">
          <strong>${text}</strong>
          <small>Just now</small>
        </div>
        <span class="transaction-amount ${outgoing ? 'out' : 'in'}">${outgoing ? '-' : '+'}${money(index % 2 === 0 ? 0.01 : 1.5)}</span>
      </div>
    `;
  }).join('');
}

function renderDashboard(account) {
  const userName = document.querySelector('#userName');
  const avatar = document.querySelector('#avatar');
  const accountLabel = document.querySelector('#accountLabel');
  const balanceValue = document.querySelector('#balanceValue');

  if (!userName || !avatar || !accountLabel || !balanceValue) return;

  userName.textContent = account.name;
  avatar.textContent = account.name.charAt(0).toUpperCase();
  accountLabel.textContent = `A/C ${account.accountNumber}`;
  balanceValue.textContent = money(account.balance);
  renderTransactions(account);
}

function initLogin() {
  const form = document.querySelector('#loginForm');
  if (!form) return;

  const createAccountButton = document.querySelector('#createAccountButton');
  if (createAccountButton) {
    createAccountButton.addEventListener('click', () => {
      document.querySelector('#loginForm').hidden = true;
      document.querySelector('#createAccountForm').hidden = false;
      document.querySelector('#newAccountName').focus();
    });
  }

  const cancelCreateAccount = document.querySelector('#cancelCreateAccount');
  if (cancelCreateAccount) {
    cancelCreateAccount.addEventListener('click', () => {
      document.querySelector('#createAccountForm').hidden = true;
      document.querySelector('#loginForm').hidden = false;
    });
  }

  const createAccountForm = document.querySelector('#createAccountForm');
  if (createAccountForm) {
    createAccountForm.addEventListener('submit', async event => {
      event.preventDefault();
      const accountName = document.querySelector('#newAccountName').value.trim();
      const newPin = document.querySelector('#newAccountPin').value.trim();
      const message = document.querySelector('#loginMessage');
      const createdAccount = document.querySelector('#createdAccount');

      if (!accountName || !/^\d{4}$/.test(newPin)) {
        createdAccount.textContent = 'Enter your name and a valid 4-digit PIN.';
        return;
      }

      try {
        const response = await apiRequest('/api/create-account', {
          method: 'POST',
          body: JSON.stringify({ name: accountName, pin: Number(newPin) })
        });
        const accountNumber = response.account?.accountNumber;
        if (!accountNumber) throw new Error('Account number was not returned.');

        document.querySelector('#createAccountForm').hidden = true;
        document.querySelector('#loginForm').hidden = false;
        message.textContent = '';
        createdAccount.textContent = '';
        document.querySelector('#generatedAccountNumber').textContent = accountNumber;
        document.querySelector('#accountCreatedCard').hidden = false;
        document.querySelector('#accountNumber').value = accountNumber;
        document.querySelector('#pin').value = newPin;
      } catch (error) {
        createdAccount.textContent = error.message || 'Unable to create an account.';
      }
    });
  }

  form.addEventListener('submit', async event => {
    event.preventDefault();

    const accountNumber = document.querySelector('#accountNumber').value.trim();
    const pin = document.querySelector('#pin').value.trim();
    const message = document.querySelector('#loginMessage');

    if (!/^\d{4}$/.test(accountNumber) || !/^\d{4}$/.test(pin)) {
      message.textContent = 'Enter a valid 4-digit account number and PIN.';
      return;
    }

    try {
      const response = await apiRequest('/api/login', {
        method: 'POST',
        body: JSON.stringify({ accountNumber: Number(accountNumber), pin: Number(pin) })
      });

      if (!response.success) {
        message.textContent = response.message || 'Account number or PIN is incorrect.';
        return;
      }

      sessionStorage.setItem(SESSION_KEY, String(response.account.accountNumber));
      window.location.href = 'hot.html';
    } catch (error) {
      message.textContent = error.message || 'Unable to connect to the banking server.';
    }
  });
}

async function reloadAccount() {
  const accountNumber = getSessionAccount();
  if (!accountNumber) {
    window.location.href = 'login page.html';
    return null;
  }

  try {
    const response = await apiRequest(`/api/account?accountNumber=${encodeURIComponent(accountNumber)}`);
    if (!response.account) {
      throw new Error('Account not found');
    }
    renderDashboard(response.account);
    return response.account;
  } catch (error) {
    showToast(error.message || 'Unable to load account');
    return null;
  }
}

function openActionModal(action) {
  const modal = document.querySelector('#actionModal');
  const fields = document.querySelector('#actionFields');
  const title = document.querySelector('#actionModalTitle');
  const description = document.querySelector('#actionModalDescription');
  const error = document.querySelector('#actionError');
  if (!modal || !fields || !title || !description || !error) return Promise.resolve(null);

  const config = {
    deposit: ['Deposit money', 'Enter the amount you want to add to your account.', '<label>Amount<input id="modalAmount" type="number" min="0.01" step="0.01" placeholder="e.g. 500" required></label>'],
    withdraw: ['Withdraw money', 'Enter the amount you want to withdraw.', '<label>Amount<input id="modalAmount" type="number" min="0.01" step="0.01" placeholder="e.g. 500" required></label>'],
    transfer: ['Transfer money', 'Enter the amount and receiver account number.', '<label>Amount<input id="modalAmount" type="number" min="0.01" step="0.01" placeholder="e.g. 500" required></label><label>Receiver account number<input id="modalReceiver" inputmode="numeric" maxlength="4" placeholder="e.g. 1002" required></label>'],
    changePin: ['Change PIN', 'Verify your current PIN and choose a new 4-digit PIN.', '<label>Current PIN<input id="modalCurrentPin" type="password" inputmode="numeric" maxlength="4" required></label><label>New 4-digit PIN<input id="modalNewPin" type="password" inputmode="numeric" maxlength="4" required></label>']
  }[action];
  if (!config) return Promise.resolve(null);

  title.textContent = config[0];
  description.textContent = config[1];
  fields.innerHTML = config[2];
  error.textContent = '';
  modal.hidden = false;
  const firstInput = fields.querySelector('input');
  if (firstInput) firstInput.focus();

  return new Promise(resolve => {
    modal.actionResolve = resolve;
    modal.actionType = action;
  });
}

function closeActionModal(result = null) {
  const modal = document.querySelector('#actionModal');
  if (!modal) return;
  modal.hidden = true;
  if (modal.actionResolve) {
    modal.actionResolve(result);
    modal.actionResolve = null;
  }
}

function showStatementModal(transactions) {
  const modal = document.querySelector('#statementModal');
  const content = document.querySelector('#statementContent');
  if (!modal || !content) return;
  content.textContent = transactions.length ? transactions.join('\n') : 'No transactions yet.';
  modal.hidden = false;
}

function closeStatementModal() {
  const modal = document.querySelector('#statementModal');
  if (modal) modal.hidden = true;
}

async function performAction(action) {
  const accountNumber = getSessionAccount();
  if (!accountNumber) {
    window.location.href = 'login page.html';
    return;
  }

  let endpoint = '';
  let payload = { accountNumber: Number(accountNumber) };

  if (['deposit', 'withdraw', 'transfer', 'changePin'].includes(action)) {
    const values = await openActionModal(action);
    if (!values) return;
    if (action === 'changePin') {
      endpoint = '/api/change-pin';
      payload.currentPin = Number(values.currentPin);
      payload.newPin = Number(values.newPin);
    } else {
      endpoint = `/api/${action}`;
      payload.amount = Number(values.amount);
      if (action === 'transfer') payload.receiverAccountNumber = Number(values.receiverNumber);
    }
  } else if (action === 'statement') {
    try {
      const response = await apiRequest(`/api/account?accountNumber=${encodeURIComponent(accountNumber)}`);
      const transactions = Array.isArray(response.account?.transactions) ? response.account.transactions : [];
      showStatementModal(transactions);
      return;
    } catch (error) {
      showToast(error.message || 'Unable to load statement');
      return;
    }
  }

  try {
    const response = await apiRequest(endpoint, {
      method: 'POST',
      body: JSON.stringify(payload)
    });

    showToast(response.message || 'Operation completed');
    renderDashboard(response.account);
  } catch (error) {
    showToast(error.message || 'Operation failed');
  }
}

function initDashboard() {
  const balanceValue = document.querySelector('#balanceValue');
  if (!balanceValue) {
    return;
  }

  const accountNumber = getSessionAccount();
  if (!accountNumber) {
    window.location.href = 'login page.html';
    return;
  }

  reloadAccount();

  const actionForm = document.querySelector('#actionForm');
  if (actionForm) {
    actionForm.addEventListener('submit', event => {
      event.preventDefault();
      const modal = document.querySelector('#actionModal');
      const error = document.querySelector('#actionError');
      const amount = document.querySelector('#modalAmount');
      const receiver = document.querySelector('#modalReceiver');
      const currentPin = document.querySelector('#modalCurrentPin');
      const newPin = document.querySelector('#modalNewPin');
      if (amount && (!Number.isFinite(Number(amount.value)) || Number(amount.value) <= 0)) {
        error.textContent = 'Enter an amount greater than zero.';
        return;
      }
      if (receiver && !/^\d{4}$/.test(receiver.value)) {
        error.textContent = 'Receiver account number must be 4 digits.';
        return;
      }
      if (newPin && !/^\d{4}$/.test(newPin.value)) {
        error.textContent = 'New PIN must be exactly 4 digits.';
        return;
      }
      closeActionModal({ amount: amount?.value, receiverNumber: receiver?.value, currentPin: currentPin?.value, newPin: newPin?.value });
      void modal;
    });
  }
  document.querySelector('#closeActionModal')?.addEventListener('click', () => closeActionModal());
  document.querySelector('#cancelActionModal')?.addEventListener('click', () => closeActionModal());
  document.querySelector('#closeStatementModal')?.addEventListener('click', closeStatementModal);
  document.querySelector('#statementDone')?.addEventListener('click', closeStatementModal);

  document.querySelectorAll('[data-action]').forEach(button => {
    button.addEventListener('click', () => performAction(button.dataset.action));
  });

  const logoutButton = document.querySelector('#logoutButton');
  if (logoutButton) {
    logoutButton.addEventListener('click', () => {
      sessionStorage.removeItem(SESSION_KEY);
      window.location.href = 'login page.html';
    });
  }

  const hideBalance = document.querySelector('#hideBalance');
  if (hideBalance) {
    hideBalance.addEventListener('click', async event => {
      const balance = document.querySelector('#balanceValue');
      const currentlyHidden = balance.textContent === '••••••';
      const account = await reloadAccount();
      if (!account) return;
      balance.textContent = currentlyHidden ? money(account.balance) : '••••••';
      event.currentTarget.innerHTML = currentlyHidden ? '◉ <span>Hide</span>' : '◉ <span>Show</span>';
    });
  }

  const viewAll = document.querySelector('#viewAll');
  if (viewAll) {
    viewAll.addEventListener('click', () => showToast('Showing your latest transactions'));
  }
}

document.addEventListener('DOMContentLoaded', () => {
  initLogin();
  initDashboard();
});
