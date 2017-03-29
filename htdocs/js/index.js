// Main
(() => {
  window.uiUpdate = (()=> {
    function setInnerText(id, text) {
      document.getElementById(id).innerText = text;
    }

    function setValue(id, value) {
      document.getElementById(id).value = value;
    }

    function setSelect(id, itemList) {
      const select = document.getElementById(id);
      select.innerHTML = "";

      for (const item of itemList) {
        const opt = document.createElement('option');
        opt.innerText = item;
        select.appendChild(opt);
      }
    }

    const _renter = (()=> {
      return {
        setAllocatedFunds: (currency)=> {
          setInnerText('ID_Renter_AllocatedFunds', currency);
        },
        setUsedFunds: (currency)=> {
          setInnerText('ID_Renter_UsedFunds', currency);
        },
        setAvailableFunds: (currency)=> {
          setInnerText('ID_Renter_AvailableFunds', currency);
        },
        setHostCount: (count)=> {
          setInnerText('ID_Renter_HostCount', count);
        },
        setEstimatedSpace: (space)=> {
          setInnerText('ID_Renter_EstimatedSpace', space);
        },
        setUsedSpace: (space)=> {
          setInnerText('ID_Renter_UsedSpace', space);
        },
        setEstimatedCost: (currency)=> {
          setInnerText('ID_Renter_EstimatedCost', currency);
        },
        setAvailableSpace: (space)=> {
          setInnerText('ID_Renter_AvailablSpace', space);
        },
        setDownloadCost: (currency)=> {
          setInnerText('ID_Renter_EstimatedDownloadCost', currency);
        },
        setUploadCost: (currency)=> {
          setInnerText('ID_Renter_EstimatedUploadCost', currency);
        },
        setAllowance: (allowance)=> {
          if (document.getElementById('renter_settings_window').classList.contains('hidden-element')) {
            setValue('ID_RenterSetFunds', allowance.Funds);
            setValue('ID_RenterSetHosts', allowance.Hosts);
            setValue('ID_RenterSetPeriod', allowance.Period);
            setValue('ID_RenterSetRenewWindow', allowance.RenewWindowInBlocks);
          }
        }
      };
    })();

    const _wallet = (()=> {
      return {
        setConfirmedBalance: (balance)=> {
          setInnerText('ID_WalletConfirmedBalance', balance);
        },
        setUnconfirmedBalance: (balance)=> {
          setInnerText('ID_WalletBalanceUnconfirmed', balance);
        },
        setTotalBalance: (balance)=> {
          setInnerText('ID_WalletTotalBalance', balance);
        },
        setReceiveAddress: (address)=> {
          setInnerText('ID_WalletReceiveAddress', address);
        }
      };
    })();

    return {
      Renter: _renter,
      Wallet: _wallet,
      setBlockHeight: (height) => {
        setInnerText('ID_BlockHeight', height);
      },
      setServerVersion: (version) => {
        setInnerText('ID_ServerVersion', version);
      },
      setAvailableDriveLetters: (drives)=> {
        if (document.getElementById('ID_MountButton').innerText !== 'Unmount') {
          setSelect('ID_MountDrives', drives);
        }
      },
      notifyDriveUnmounted: _notifyDriveUnmounted
    };
  })();


  const UiState = (()=> {
    function _clientVersion() {
      return 'v' + window.uiState.clientVersion;
    }

    function _isOnline() {
      return window.uiState.isOnline;
    }

    function _isWalletConfigured() {
      return window.uiState.isWalletConfigured;
    }

    function _isWalletLocked() {
      return window.uiState.isWalletLocked;
    }

    return {
      clientVersion: _clientVersion,
      isOnline: _isOnline,
      isWalletConfigured: _isWalletConfigured,
      isWalletLocked: _isWalletLocked
    };
  })();

  const AppActions = (() => {
    function _createWallet(cb) {
      console.log('Create wallet');
      return window.appActions.createWallet(cb);
    }
    
    function _mountDrive(mountLocation, cb) {
      console.log('Mount drive: ' + mountLocation);
      return window.appActions.mountDrive(mountLocation, cb);
    }

    function _startApp() {
      window.appActions.startApp();
    }

    function _stopApp() {
      window.appActions.stopApp();
    }

    function _unlockWallet(pwd, cb) {
      return window.appActions.unlockWallet(pwd, cb);
    }

    function _unmountDrive(cb) {
      console.log('Unmount drive');
      return window.appActions.unmountDrive(cb);
    }

    function _shutdown() {
      window.appActions.shutdown();
    }

    function _setRenterAllowance(allowance) {
      window.appActions.setRenterAllowance(allowance);
    }

    return {
      createWallet: _createWallet,
      mountDrive: _mountDrive,
      startApp: _startApp,
      stopApp: _stopApp,
      unlockWallet: _unlockWallet,
      unmountDrive: _unmountDrive,
      shutdown: _shutdown,
      setRenterAllowance: _setRenterAllowance
    };
  })();

  let _mountHandler;

  function setMainWindow(name) {
    console.log('Setting main window: ' + name);
    const elem = document.getElementById(name);
    const mainWindow = document.getElementById('main_window');
    if (mainWindow.childElementCount === 1) {
      const curElem = mainWindow.firstChild;
      mainWindow.removeChild(curElem);
      curElem.classList.add('hidden-element');
      document.body.appendChild(curElem);
    }

    elem.parentElement.removeChild(elem);
    elem.classList.remove('hidden-element');
    mainWindow.appendChild(elem);
  }

  function displayErrorPopup(title, msg, cb) {
    alert(msg);
    if (cb) {
      cb();
    }
  }

  function _notifyDriveUnmounted() {
    const mountButton = document.getElementById('ID_MountButton');
    const mountSelect = document.getElementById('ID_MountDrives');
    if (mountButton.innerText === 'Unmount') {
      mountButton.innerText = "Mount";
      mountButton.onclick = _mountHandler;
      mountButton.disabled = false;
      mountSelect.disabled = false;
    }
  }

  function beginMainApplication() {
    AppActions.startApp();
    setMainWindow('app_window');
    const renterEditLink = document.getElementById('ID_Renter_Edit');
    renterEditLink.onclick = ()=> {
      renterEditLink.onclick = null;
      handleRenterEditSettings();
    };
    const mountButton = document.getElementById('ID_MountButton');
    const mountSelect = document.getElementById('ID_MountDrives');

    _mountHandler = ()=> {
      mountButton.onclick = null;
      mountButton.disabled = true;
      mountSelect.disabled = true;
      if (mountButton.innerText === "Mount") {
        AppActions.mountDrive(mountSelect.value, (success, reason) => {
          if (success) {
            mountButton.innerText = "Unmount";
          } else {
            displayErrorPopup('Mount Failed', reason);
          }
          mountButton.onclick = _mountHandler;
          mountButton.disabled = false;
        });
      } else {
        AppActions.unmountDrive((success, reason) => {
          if (success) {
            _notifyDriveUnmounted()
          } else {
            displayErrorPopup('Unmount Failed', reason);
            mountButton.onclick = _mountHandler;
            mountButton.disabled = false;
          }
        });
      }
    };
    mountButton.onclick = _mountHandler;
  }

  function handleRenterEditSettings() {

    setMainWindow('renter_settings_window');
    const cancelButton = document.getElementById('ID_RenterSettingsCancel');
    cancelButton.onclick = ()=> {
      cancelButton.onclick = null;
      beginMainApplication();
    }
  }

  function handleUnlockWallet() {
    setMainWindow('unlock_window');

    const unlockButton = document.getElementById('ID_UnlockWalletButton');
    unlockButton.onclick = () => {
      unlockButton.onclick = null;
      const password = document.getElementById('ID_WalletUnlockPwd');
      if (AppActions.unlockWallet(password.value, (success, reason) => {
          if (success) {
            beginMainApplication();
          } else {
            displayErrorPopup('Error', reason, () => {
              handleUnlockWallet();
            });
          }
        })) {
        password.value = '';
        setMainWindow('unlocking_window');
      } else {
        password.value = '';
      }
    };
  }

  function handleWalletCreated(seed) {
    setMainWindow('wallet_created_window');
    document.getElementById('ID_WalletSeed').innerText = seed;
    const button = document.getElementById('ID_WalletCreatedButton');
    button.onclick = ()=> {
      button.onclick = null;
      handleUnlockWallet();
    };
  }

  function handleCreateWallet() {
    setMainWindow('create_window');
    const createButton = document.getElementById('ID_CreateWalletButton');
    createButton.onclick = () => {
      createButton.onclick = null;
      AppActions.createWallet((success, reasonOrSeed) => {
        if (success) {
          handleWalletCreated(reasonOrSeed);
        } else {
          displayErrorPopup('Error', reasonOrSeed, () => {
            handleCreateWallet();
          });
        }
      });
    };
  }

  window.addEventListener('load', ()=> {
    console.log('Main window load');
    AppActions.stopApp();
    document.getElementById('ID_SiaDrive').innerText = 'SiaDrive ' + UiState.clientVersion();
    document.getElementById('ID_ServerVersion').innerText = '...';
    if (UiState.isOnline()) {
      if (UiState.isWalletConfigured()) {
        if (UiState.isWalletLocked()) {
          handleUnlockWallet();
        } else {
          beginMainApplication();
        }
      } else {
        handleCreateWallet();
      }
    } else {
      setMainWindow('offline_window');
    }
  });

  window.onunload = ()=> {
    AppActions.shutdown();
  };
})();