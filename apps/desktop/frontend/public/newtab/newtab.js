(() => {
  'use strict';

  const input = document.getElementById('q');
  if (!(input instanceof HTMLInputElement)) {
    return;
  }

  input.focus();

  document.addEventListener('keydown', (event) => {
    if (event.key !== '/' || event.ctrlKey || event.metaKey || event.altKey) {
      return;
    }
    if (document.activeElement === input) {
      return;
    }
    event.preventDefault();
    input.focus();
    input.select();
  });
})();
