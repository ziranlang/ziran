(function () {
  var root = document.documentElement;
  var saved;
  var paperPreview = root.dataset.embedTheme === 'waozi';
  if (paperPreview) root.dataset.theme = 'light';
  else {
    try { saved = localStorage.getItem('ziran-theme'); } catch (_) {}
    if (saved === 'dark' || saved === 'light') root.dataset.theme = saved;
  }
  var toggle = document.querySelector('.theme-toggle');
  function updateThemeLabel() {
    if (toggle) toggle.setAttribute('aria-label', root.dataset.theme === 'dark' ? 'Switch to light theme' : 'Switch to dark theme');
  }
  updateThemeLabel();
  if (toggle) toggle.addEventListener('click', function () {
    var next = root.dataset.theme === 'dark' ? 'light' : 'dark';
    root.dataset.theme = next;
    try { localStorage.setItem('ziran-theme', next); } catch (_) {}
    updateThemeLabel();
  });
  var menu = document.querySelector('.menu-button');
  var nav = document.getElementById('primary-nav');
  function closeMenu() {
    if (!menu || !nav) return;
    menu.setAttribute('aria-expanded', 'false');
    nav.classList.remove('is-open');
  }
  if (menu && nav) menu.addEventListener('click', function () {
    var open = menu.getAttribute('aria-expanded') !== 'true';
    menu.setAttribute('aria-expanded', String(open));
    nav.classList.toggle('is-open', open);
  });
  if (nav) nav.addEventListener('click', function (event) {
    if (event.target.closest('a')) closeMenu();
  });
  document.addEventListener('keydown', function (event) {
    if (event.key === 'Escape') closeMenu();
  });
  var exampleTabs = document.querySelector('.example-tabs');
  if (exampleTabs) {
    var tabs = Array.from(exampleTabs.querySelectorAll('[role="tab"]'));
    var exampleLink = document.querySelector('.example-link');
    var exampleAnchors = { 'example-hello': 'hello', 'example-score': 'score', 'example-text': 'text-demo' };
    function selectExample(tab) {
      tabs.forEach(function (item) {
        var selected = item === tab;
        item.setAttribute('aria-selected', String(selected));
        item.tabIndex = selected ? 0 : -1;
        document.getElementById(item.getAttribute('aria-controls')).hidden = !selected;
      });
      if (exampleLink) exampleLink.href = 'examples.html#' + exampleAnchors[tab.getAttribute('aria-controls')];
      exampleTabs.dispatchEvent(new Event('examplechange'));
    }
    tabs.forEach(function (tab, index) {
      tab.addEventListener('click', function () { selectExample(tab); });
      tab.addEventListener('keydown', function (event) {
        var next;
        if (event.key === 'ArrowRight') next = (index + 1) % tabs.length;
        else if (event.key === 'ArrowLeft') next = (index + tabs.length - 1) % tabs.length;
        else if (event.key === 'Home') next = 0;
        else if (event.key === 'End') next = tabs.length - 1;
        else return;
        event.preventDefault();
        selectExample(tabs[next]);
        tabs[next].focus();
      });
    });
    exampleTabs.hidden = false;
  }
  var dialog = document.querySelector('.support-dialog');
  if (dialog && dialog.showModal) {
    var dialogTitle = dialog.querySelector('h3');
    var dialogQr = dialog.querySelector('.support-qr');
    var dialogAddress = dialog.querySelector('.support-full');
    var copyButton = dialog.querySelector('.support-copy');
    var copyStatus = dialog.querySelector('.support-status');
    document.querySelectorAll('.support-address').forEach(function (button) {
      button.addEventListener('click', function () {
        dialogTitle.textContent = button.dataset.coin;
        dialogQr.src = button.dataset.qr;
        dialogQr.alt = 'QR code for the ' + button.dataset.coin + ' address';
        dialogAddress.textContent = button.dataset.address;
        copyButton.textContent = 'Copy address';
        copyStatus.textContent = '';
        dialog.showModal();
        dialog.focus();
      });
    });
    copyButton.addEventListener('click', function () {
      var address = dialogAddress.textContent;
      function fallback() {
        var range = document.createRange();
        range.selectNodeContents(dialogAddress);
        var selection = window.getSelection();
        selection.removeAllRanges();
        selection.addRange(range);
        copyStatus.textContent = 'Address selected. Press Ctrl+C to copy it.';
      }
      if (!navigator.clipboard) { fallback(); return; }
      navigator.clipboard.writeText(address).then(function () {
        copyButton.textContent = 'Copied';
        copyStatus.textContent = 'Address copied.';
      }, fallback);
    });
    dialog.querySelector('.support-close').addEventListener('click', function () { dialog.close(); });
    dialog.addEventListener('click', function (event) {
      if (event.target === dialog) dialog.close();
    });
  }
  document.querySelectorAll('.article-body pre').forEach(function (block) {
    var button = document.createElement('button');
    button.type = 'button';
    button.className = 'copy-button';
    button.textContent = 'Copy';
    button.setAttribute('aria-label', 'Copy code');
    button.addEventListener('click', function () {
      if (!navigator.clipboard) return;
      navigator.clipboard.writeText(block.querySelector('code').textContent).then(function () {
        button.textContent = 'Copied';
        setTimeout(function () { button.textContent = 'Copy'; }, 1800);
      });
    });
    block.appendChild(button);
  });
})();
