#!/bin/sh
# CLIOPLAY Bluetooth bootstrap - AUTORECONNECT2 EARLYBOOT + PAIRFIX26 DEBUGGATE + MULTI-DEVICE+KEYVAULT+AVRCP-SDP+PIN0000 REMOTE-FIRST
# Preserve the exact known-good Renault HID startup path. A2DP/phone pairing
# is layered on only after hidd has started. Phone pairing uses TomTom's factory blueserver/hcid runtime pairing gate; hcid/hidd are never restarted for pairing.
LOG=/media/sdcard/clio843/bt.log
DEBUG=${CLIO_DEBUG:-0}
SD=/mnt/sdcard/clio843
HREQ=/tmp/clbthci
# Volatile boot barrier: never accept a marker left by an earlier launch.
BOOT_READY=/tmp/clbtrdy
rm -f "$BOOT_READY" >/dev/null 2>&1

# clio040 mirrors the live Settings > Debug state here once the UI starts.
# Before then, keep using the boot-time CLIO_DEBUG value supplied by ttn.
debug_now(){
  if [ -r /tmp/cldebug ]; then
    D=`cat /tmp/cldebug 2>/dev/null`
    [ "$D" = "1" ]
  else
    [ "$DEBUG" = "1" ]
  fi
}

if [ "$DEBUG" = "1" ]; then
  : > "$LOG"
  OUT="$LOG"
else
  rm -f "$LOG" >/dev/null 2>&1
  OUT=/dev/null
fi
log(){ if debug_now; then echo "$1" >> "$LOG"; fi; }

log "BT START AUTORECONNECT2 EARLYBOOT PAIRFIX26 DEBUGGATE MULTI-DEVICE+KEYVAULT+AVRCP-SDP+PIN0000 REMOTE-FIRST"

if [ -r /proc/barcelona/export_hw_var ]; then
  . /proc/barcelona/export_hw_var
fi

# ---- KNOWN-GOOD CLIOPLAY HID BOOT PATH (kept intentionally unchanged) ----
READY=0
N=0
while [ $N -lt 3 ]; do
  if hciconfig hci0 2>/dev/null | grep -q "UP"; then
    READY=1
    break
  fi
  sleep 1
  N=`expr $N + 1`
done

if [ "$READY" != "1" ]; then
  log "BT FULL INIT"
  killall hidd >/dev/null 2>&1
  killall hciattach >/dev/null 2>&1
  killall sdpd >/dev/null 2>&1
  killall hcid >/dev/null 2>&1
  sleep 1

  sed "s/@DEVNAME@/${hw_btname}/;s/@DEVCLASS@/${hw_btclass}/" \
    /etc/bluetooth/hcid.conf.template > /etc/bluetooth/hcid.conf
  hcid >> "$OUT" 2>&1 || log "HCID FAIL"
  sdpd >> "$OUT" 2>&1 || log "SDPD FAIL"

  if [ "${hw_btusb}" != "0" ]; then
    insmod ohci-hcd.ko >> "$OUT" 2>&1
    sleep 1
  fi

  if [ "${hw_btusb}" = "0" ]; then
    csrinit bcsp /dev/bt ${hw_btspeed} ${hw_btclock} >> "$OUT" 2>&1 || log "CSR FAIL"
    hciattach /dev/bt bcsp ${hw_btspeed} >> "$OUT" 2>&1 || log "HCIATT FAIL"
  else
    csrinit usb /dev/bt ${hw_btspeed} ${hw_btclock} >> "$OUT" 2>&1 || \
      csrinit bcsp /dev/bt ${hw_btspeed} ${hw_btclock} >> "$OUT" 2>&1
  fi
  hciconfig hci0 up >> "$OUT" 2>&1 || log "HCIUP FAIL"
else
  log "BT REUSE HCI0"
fi

# This is the critical remote path from the original working CLIOPLAY script.
if ! pidof hidd >/dev/null 2>&1; then
  "$SD/hidd" --server -n >> "$OUT" 2>&1 &
  log "HIDD STARTED"
else
  log "HIDD ALREADY"
fi

# Give the Renault HID server a quiet window to accept/reconnect the remote
# before adding any phone-facing profile/service work.
sleep 3
if [ "$DEBUG" = "1" ]; then
  "$SD/hidd" --show >> "$OUT" 2>&1
fi
log "REMOTE WINDOW COMPLETE"

# ---- PHONE/A2DP LAYER: never bounce HCI, hcid or hidd ----
# Present the adapter as an actual car-audio renderer once the Renault HID
# remote has had its quiet reconnect window.  sdptool does NOT update CoD.
# 0x240420 = Audio + Rendering service, Audio/Video major, Car Audio minor.
hciconfig hci0 class 0x240420 >> "$OUT" 2>&1 && log "BT CLASS CAR-AUDIO 0x240420" || log "BT CLASS SET WARN"
# Stock rc.bluetooth normally already owns hcid+sdpd.  If sdpd alone is absent,
# starting it here is profile-discovery only and does not touch the controller.
if ! pidof sdpd >/dev/null 2>&1; then
  sdpd >> "$OUT" 2>&1 &
  sleep 1
  log "SDPD STARTED LATE"
fi

# Ensure TomTom's own Bluetooth policy server is available.  The stock UI uses
# /var/run/bluesock to toggle hcid pairing acceptance with PAIRING ENABLE/DISABLE.
# Starting this after the HID quiet window does not reset hci0 or touch hidd.
if ! pidof blueserver.a >/dev/null 2>&1; then
  rm -f /var/run/bluesock >/dev/null 2>&1
  /bin/blueserver.a /var/run/bluesock >> "$OUT" 2>&1 &
  sleep 1
  log "BLUESERVER STARTED LATE"
else
  log "BLUESERVER READY"
fi

# Advertise our sink only after the remote startup window.
sdptool add A2SNK >> "$OUT" 2>&1 || log "A2SNK SDP ADD WARN"
log "A2DP SBC CAPS 44K1 STEREO+JOINT BITPOOL2-53"
# This old sdptool has no AVRCT template. clioa2dp registers the AVRCP
# Controller record directly with the local SDP server and owns PSM 0x17.
if [ "$DEBUG" = "1" ]; then
  hciconfig hci0 >> "$OUT" 2>&1
  sdptool browse local >> "$OUT" 2>&1
fi

# Publish the adapter address for clioa2dp's binary hcid key-database sync.
# TomTom's hcid stores records keyed by both local and remote BDADDR.
hciconfig hci0 2>/dev/null | sed -n 's/.*BD Address: \([0-9A-Fa-f:]*\).*/\1/p' > /tmp/clbtlocal
if [ -s /tmp/clbtlocal ]; then log "BT LOCAL BDADDR `cat /tmp/clbtlocal`"; fi

rm -f /tmp/clbtstat /tmp/clbtmeta /tmp/clbtdevices /tmp/clbtcmd "$HREQ" >/dev/null 2>&1
if ! pidof clioa2dp >/dev/null 2>&1; then
  "$SD/clioa2dp" >> "$OUT" 2>&1 &
  sleep 1
  log "CLIOA2DP STARTED"
else
  log "CLIOA2DP ALREADY"
fi

# Address-specific phone helper.  Normal PAIR only changes scan mode because
# clioa2dp has already sent TomTom's factory PAIRING ENABLE|<fixed PIN 0000> command over
# /var/run/bluesock.  *_FALLBACK reproduces blueserver's SIGUSR/client_pin
# actions only if that factory socket is unavailable.
(
  while pidof clioa2dp >/dev/null 2>&1; do
    if [ -s "$HREQ" ]; then
      LINE=`cat "$HREQ" 2>/dev/null`
      rm -f "$HREQ" >/dev/null 2>&1
      set -- $LINE
      CMD="$1"
      ADDR="$2"
      case "$CMD" in
        PAIR)
          hciconfig hci0 piscan >> "$OUT" 2>&1
          log "PHONE PAIR SCAN STOCK-POLICY"
          ;;
        PAIR_FALLBACK)
          PIN=0000
          echo "$PIN" > /etc/bluetooth/client_pin
          killall -SIGUSR2 hcid >/dev/null 2>&1
          hciconfig hci0 piscan >> "$OUT" 2>&1
          log "PHONE PAIR POLICY FALLBACK ENABLED PIN $PIN"
          ;;
        PAIR_END)
          hciconfig hci0 pscan >> "$OUT" 2>&1
          log "PHONE PAIR SCAN ENDED"
          ;;
        PAIR_END_FALLBACK)
          rm -f /etc/bluetooth/client_pin >/dev/null 2>&1
          killall -SIGUSR1 hcid >/dev/null 2>&1
          hciconfig hci0 pscan >> "$OUT" 2>&1
          log "PHONE PAIR POLICY FALLBACK DISABLED"
          ;;
        CONNECT)
          # PAIRFIX23: reconnect never enables pairing policy; clioa2dp restores the saved link key before opening the media profile.
          hciconfig hci0 pscan >> "$OUT" 2>&1
          if [ -n "$ADDR" ]; then
            if hcitool con 2>/dev/null | grep -qi "$ADDR"; then
              log "PHONE RECONNECT ACL ALREADY UP $ADDR"
            else
              hcitool cc "$ADDR" >> "$OUT" 2>&1
              RC=$?
              log "PHONE RECONNECT ACL RC=$RC $ADDR"
            fi
          fi
          log "PHONE RECONNECT BOND-ONLY $ADDR"
          ;;
        CONNECT_FALLBACK)
          hciconfig hci0 pscan >> "$OUT" 2>&1
          if [ -n "$ADDR" ]; then hcitool cc "$ADDR" >> "$OUT" 2>&1; fi
          log "PHONE RECONNECT BOND-ONLY FALLBACK $ADDR"
          ;;
        DISCONNECT)
          if [ -n "$ADDR" ]; then hcitool dc "$ADDR" 0x13 >> "$OUT" 2>&1; fi
          log "PHONE DISCONNECT $ADDR"
          ;;
        UNPAIR)
          if [ -n "$ADDR" ]; then
            hcitool dc "$ADDR" 0x13 >> "$OUT" 2>&1
            hciconfig hci0 delkey "$ADDR" >> "$OUT" 2>&1
          fi
          # clioa2dp removes the matching 36-byte record from TomTom hcid's
          # binary /etc/bluetooth/link_key database and from btkeys.dat.
          hciconfig hci0 piscan >> "$OUT" 2>&1
          log "PHONE UNPAIR $ADDR"
          ;;
      esac
    fi
    sleep 1
  done
) &

# HID/remote startup, hcid, sdpd, A2DP registration and command helper
# have all been launched by this point. The daemon separately waits only for
# its own listeners/AVRCP SDP registration plus this Bluetooth boot barrier.
# Neither the UI nor GPS participates in reconnect readiness.
if hciconfig hci0 2>/dev/null | grep -q "UP" && \
   pidof hidd >/dev/null 2>&1 && \
   pidof hcid >/dev/null 2>&1 && \
   pidof sdpd >/dev/null 2>&1 && \
   pidof clioa2dp >/dev/null 2>&1 && \
   [ -s /tmp/clbtlocal ]; then
  echo 1 > "$BOOT_READY"
  log "BT BOOT BARRIER READY - HID/HCI/SDP/HELPER"
else
  log "BT BOOT BARRIER INCOMPLETE - AUTO RECONNECT SKIPPED"
fi

log "BT READY EARLYBOOT+REMOTE-FIRST+A2DP+AVRCP-SDP+MULTIDEVICE+KEYVAULT+PAIRFIX23"
