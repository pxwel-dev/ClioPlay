/* focusd: Ghidra decompilation REFERENCE, not original source.
 * Decompiled bodies are preserved verbatim, including warnings and unknown types.
 * Function boundaries, types, names and syscall recovery may be wrong.
 * This file cannot rebuild the executable and has no equivalence guarantee.
 * The maintainer identifies the original helper as project work; dependency notices remain applicable.
 * See procedure-index.json, coverage.json and original REA evidence records.
 */
#error "Ghidra reference listing: not compilable recovered source"

/* ===== 0x20528 | start_c | ev_1576d11ac3dafa7ecb45e82abb28a3ad25bf693376e2ef7f445e300f35cf2c75 ===== */

/* WARNING: Removing unreachable block (ram,0x000207dc) */
/* WARNING: Removing unreachable block (ram,0x00020628) */
/* WARNING: Removing unreachable block (ram,0x000205e0) */
/* WARNING: Removing unreachable block (ram,0x00020674) */
/* WARNING: Removing unreachable block (ram,0x000207d8) */
/* WARNING: Removing unreachable block (ram,0x00021790) */
/* WARNING: Removing unreachable block (ram,0x00020998) */
/* WARNING: Removing unreachable block (ram,0x000209c8) */
/* WARNING: Removing unreachable block (ram,0x000209dc) */
/* WARNING: Removing unreachable block (ram,0x000209ec) */
/* WARNING: Removing unreachable block (ram,0x000209f0) */
/* WARNING: Removing unreachable block (ram,0x000209f8) */
/* WARNING: Removing unreachable block (ram,0x00020a0c) */

void start_c(void)

{
  byte bVar1;
  byte bVar2;
  char cVar3;
  bool bVar4;
  bool bVar5;
  bool bVar6;
  bool bVar7;
  uint uVar8;
  int iVar9;
  char cVar10;
  byte bVar11;
  int iVar12;
  char *pcVar13;
  undefined1 *puVar14;
  uint uVar15;
  byte *pbVar16;
  undefined1 uVar17;
  uint uVar18;
  undefined4 uVar19;
  undefined1 uVar20;
  undefined4 uVar21;
  undefined1 *puVar22;
  uint uVar23;
  uint uVar24;
  uint uVar25;
  uint uVar26;
  char *pcVar27;
  uint uVar28;
  uint uVar29;
  bool bVar30;
  uint local_588;
  undefined2 local_582;
  char local_580 [17];
  undefined1 local_56f;
  undefined4 local_514;
  undefined4 local_510;
  undefined4 local_50c;
  undefined1 local_111;
  undefined1 local_110;
  undefined1 local_10f;
  undefined1 local_10e;
  undefined1 local_10d;
  undefined4 *local_10c [65];
  
  iVar12 = 0;
  do {
    *(undefined1 *)((int)&local_582 + iVar12) = 0;
    iVar12 = iVar12 + 1;
  } while (iVar12 != 0x6e);
  iVar12 = 0;
  local_582 = 1;
  do {
    local_580[iVar12] = "/var/run/PAC09900"[iVar12];
    iVar12 = iVar12 + 1;
  } while (iVar12 != 0x11);
  local_56f = 0;
  software_interrupt(0x90000a);
  software_interrupt(0x90000a);
  software_interrupt(0x90000a);
  log_line("--- RUNTIME0310 PAC FOCUS ---");
  publish();
  local_50c = 0;
  local_510 = (undefined2 *)0x5;
  local_514 = 1;
  software_interrupt(0x900066);
  log_num("PAC_SERVER_CREATE",1,5,0,0);
  local_514 = 1;
  local_50c = 0x14;
  software_interrupt(0x900066);
  local_510 = &local_582;
  log_num("PAC_BIND",2,0x14,0,0);
  local_514 = 1;
  local_510 = (undefined2 *)0x4;
  software_interrupt(0x900066);
  log_num("PAC_LISTEN",4,4,0,0);
  pcVar13 = (char *)0x241;
  iVar12 = 0x1b6;
  software_interrupt(0x900005);
  for (pcVar27 = (char *)0x0; pcVar27 < (char *)0x6; pcVar27 = "/tmp/cfready" + (int)pcVar27) {
    pcVar13 = "READY\n" + (int)pcVar27;
    iVar12 = 6 - (int)pcVar27;
    software_interrupt(0x900004);
  }
  software_interrupt(0x900006);
  log_line("PAC_READY",pcVar13,iVar12);
  local_10c[2] = (undefined4 *)0x0;
  local_10c[1] = (undefined4 *)0x0;
  local_10c[0] = (undefined4 *)0x1;
  software_interrupt(0x900066);
  log_num("PAC_ACCEPT_OK",5,0,0,0);
  software_interrupt(0x900066);
  do {
    local_10c[3] = (undefined4 *)0x0;
    local_10c[2] = (undefined4 *)0x400;
    local_588 = 2;
    uVar8 = local_514 >> 0x10 & 0x7f;
    if (((0x13 < uVar8) || ((1 << uVar8 & 0xe687eU) == 0)) && (uVar8 != 0x7f)) {
      uVar8 = (local_514 & 0x7f) - 1;
      if (((0x12 < uVar8) || (local_588 = 0, (0x7343fU >> (uVar8 & 0xff) & 1) == 0)) &&
         (local_588 = 0, (local_514 & 0x7f) != 0x7f)) {
        local_588 = 2;
      }
    }
    g_rx = g_rx + 1;
    local_10c[0] = (undefined4 *)0x235852;
    local_10c[1] = &local_514;
    iVar12 = dec_append(local_10c,3,0x104);
    if (iVar12 < 0x103) {
      uVar8 = 0x102 - iVar12;
      uVar20 = 0x20;
      if (1 < uVar8) {
        uVar8 = 2;
      }
      iVar9 = uVar8 + 1;
      puVar14 = &DAT_00010208;
      do {
        *(undefined1 *)((int)local_10c + iVar12) = uVar20;
        uVar20 = *puVar14;
        iVar9 = iVar9 + -1;
        iVar12 = iVar12 + 1;
        puVar14 = puVar14 + 1;
      } while (iVar9 != 0);
    }
    iVar12 = dec_append(local_10c,iVar12,0x104,10);
    if (iVar12 < 0x103) {
      uVar8 = 0x102 - iVar12;
      uVar20 = 0x20;
      if (4 < uVar8) {
        uVar8 = 5;
      }
      iVar9 = uVar8 + 1;
      puVar14 = &DAT_00010350;
      do {
        *(undefined1 *)((int)local_10c + iVar12) = uVar20;
        uVar20 = *puVar14;
        iVar9 = iVar9 + -1;
        iVar12 = iVar12 + 1;
        puVar14 = puVar14 + 1;
      } while (iVar9 != 0);
    }
    iVar12 = dec_append(local_10c,iVar12,0x104,local_588);
    if (iVar12 < 0x103) {
      *(undefined1 *)((int)local_10c + iVar12) = 0x3a;
      iVar12 = iVar12 + 1;
    }
    iVar9 = 0;
    do {
      iVar12 = hexbyte_append(local_10c,iVar12,0x104,*(undefined1 *)((int)&local_514 + iVar9));
      iVar9 = iVar9 + 1;
    } while (iVar9 != 10);
    *(undefined1 *)((int)local_10c + iVar12) = 0;
    log_line(local_10c);
    uVar8 = local_588;
    if (local_588 < 10) {
      do {
        uVar18 = 10;
        bVar11 = *(byte *)((int)&local_514 + uVar8);
        uVar24 = 10 - uVar8;
        uVar28 = bVar11 & 0x7f;
        if (0x12 < uVar28 - 1) {
switchD_00020a60_caseD_2:
          pcVar13 = "UNHANDLED_METHOD";
          uVar15 = (uint)bVar11;
          uVar29 = uVar8;
          uVar18 = uVar24;
LAB_00020804:
          log_num(pcVar13,uVar28,uVar15,uVar29,uVar18);
          break;
        }
        iVar12 = uVar8 - 0x514;
        switch(uVar28) {
        case 1:
          if (3 < (int)uVar24) {
            uVar20 = *(undefined1 *)((int)&local_514 + uVar8 + 1);
            bVar1 = *(byte *)((int)&local_514 + uVar8 + 2);
            bVar2 = *(byte *)((int)&local_514 + uVar8 + 3);
            log_num(&DAT_00010248,uVar8,1,4,bVar11 >> 7);
            log_num("CONNECT REQUEST",uVar20,bVar1,bVar2,local_588);
            uVar20 = 2;
            local_111 = 2;
            if (bVar2 != 0) {
              uVar20 = 1;
            }
            if (1 < bVar1) {
              uVar20 = 2;
            }
            uVar17 = uVar20;
            if (1 < bVar2) {
              uVar17 = 2;
            }
            uVar19 = 2;
            if (2 < bVar1) {
              uVar17 = uVar20;
            }
            puVar14 = &local_110;
            if (local_588 == 2) {
              local_10f = 2;
              local_111 = (undefined1)local_514;
              uVar19 = 4;
              local_110 = local_514._1_1_;
              puVar14 = &local_10e;
            }
            *puVar14 = uVar17;
            local_10c[3] = (undefined4 *)0x4000;
            local_10c[0] = (undefined4 *)0x5;
            software_interrupt(0x900066);
            local_10c[1] = (undefined4 *)&local_111;
            local_10c[2] = (undefined4 *)uVar19;
            log_num("CONNECT_RESPONSE",uVar17,9,uVar19,local_588);
            log_tx("TX CONNECT_RESPONSE",&local_111,uVar19,9);
            uVar29 = 4;
            break;
          }
          pcVar13 = "TRUNC_CONNECT";
LAB_0002173c:
          uVar15 = 0;
          uVar18 = 0;
          uVar28 = uVar8;
          uVar29 = uVar24;
          goto LAB_00021750;
        default:
          goto switchD_00020a60_caseD_2;
        case 3:
          log_num(&DAT_00010248,uVar8,3,1,bVar11 >> 7);
          log_num("DISCONNECT_REQUEST",local_588,10,0,0);
          local_10c[2] = (undefined4 *)0x1;
          local_111 = 4;
          if (local_588 == 2) {
            local_10f = 4;
            local_10c[2] = (undefined4 *)0x3;
            local_110 = local_514._1_1_;
            local_111 = (undefined1)local_514;
          }
          local_10c[3] = (undefined4 *)0x4000;
          local_10c[1] = (undefined4 *)&local_111;
          local_10c[0] = (undefined4 *)0x5;
          software_interrupt(0x900066);
          log_tx("TX DISCONNECT_RESPONSE",local_10c[1],local_10c[2],9);
          uVar29 = 1;
          break;
        case 5:
          if ((int)uVar24 < 2) {
            pcVar13 = "TRUNC_FULLMODE";
            goto LAB_0002173c;
          }
          uVar20 = *(undefined1 *)((int)&local_514 + uVar8 + 1);
          log_num(&DAT_00010248,uVar8,5,2,bVar11 >> 7);
          log_num("REQUEST_FULL_MODE",uVar20,local_588,10,0);
          if (local_588 == 2) {
            local_10c[2] = (undefined4 *)0x5;
            local_10f = 0x7f;
            local_110 = local_514._1_1_;
            puVar14 = &local_10e;
            puVar22 = &local_10d;
            local_111 = (undefined1)local_514;
          }
          else {
            local_10c[2] = (undefined4 *)0x3;
            local_111 = 0x7f;
            puVar14 = &local_110;
            puVar22 = &local_10f;
          }
          *puVar22 = 0;
          local_10c[1] = (undefined4 *)&local_111;
          *puVar14 = 5;
          local_10c[3] = (undefined4 *)0x4000;
          local_10c[0] = (undefined4 *)0x5;
          software_interrupt(0x900066);
          log_tx("TX F_REPLY_FULL_MODE",local_10c[1],local_10c[2],9);
          log_num("FULL_MODE_REPLY",uVar20,9,local_588,0);
          uVar29 = 2;
          break;
        case 6:
          uVar29 = 1;
          log_num(&DAT_00010248,uVar8,6,1,bVar11 >> 7);
          pcVar13 = "RELEASE_FULL_MODE";
          uVar15 = local_588;
          goto LAB_00020dd0;
        case 0xb:
          if ((int)uVar24 < 3) {
            pcVar13 = "TRUNC_TEMPLATE";
            goto LAB_0002173c;
          }
          uVar15 = (uint)*(byte *)((int)&local_514 + uVar8 + 1);
          uVar18 = (uint)*(byte *)((int)&local_514 + uVar8 + 2);
          uVar29 = 3;
          log_num(&DAT_00010248,uVar8,0xb,3,bVar11 >> 7);
          pcVar13 = "TEMPLATE";
          g_viewer = uVar15;
          g_template = uVar18;
          goto LAB_00020dd0;
        case 0xd:
          if ((int)uVar24 < 5) {
            pcVar13 = "TRUNC_SETPROP_HDR";
            goto LAB_0002173c;
          }
          uVar26 = (uint)*(byte *)((int)&local_510 + uVar8);
          uVar29 = uVar26 + 5;
          if (uVar24 < uVar29) {
            log_num("TRUNC_SETPROP_DATA",uVar8,uVar24,uVar29,uVar26);
            goto LAB_00021754;
          }
          uVar18 = (uint)*(byte *)((int)&local_514 + uVar8 + 1);
          uVar15 = (uint)*(byte *)((int)&local_514 + uVar8 + 2);
          cVar10 = *(char *)((int)&local_514 + uVar8 + 3);
          g_viewer = uVar18;
          log_num(&DAT_00010248,uVar8,0xd,uVar29,bVar11 >> 7);
          log_num("SETPROP",uVar18,uVar15,cVar10,uVar26);
          pbVar16 = (byte *)((int)&local_510 + uVar8 + 1);
          if ((uVar15 == 0x14 && cVar10 == '\x01') && (uVar26 != 0)) {
            if (*pbVar16 == 0x2d) {
              iVar12 = -1;
              uVar15 = 1;
            }
            else if (*pbVar16 == 0x2b) {
              uVar15 = 1;
              iVar12 = 1;
            }
            else {
              iVar12 = 1;
              uVar15 = 0;
            }
            if (uVar15 < uVar26) {
              iVar9 = 0;
              uVar25 = 0;
              while (uVar23 = (uint)*(byte *)((int)&local_510 + uVar25 + uVar15 + uVar8 + 1),
                    0xfffffff5 < uVar23 - 0x3a) {
                iVar9 = uVar23 + iVar9 * 10 + -0x30;
                if ((uVar26 <= uVar15 + 1 + uVar25) ||
                   (bVar30 = 2 < uVar25, uVar25 = uVar25 + 1, bVar30)) goto LAB_000215a0;
              }
              if (uVar25 == 0) break;
LAB_000215a0:
              g_temp = iVar9 * iVar12;
              log_num(&DAT_00010462,uVar18,0x14,1,g_temp);
              publish();
              log_num("PUBLISH_TEMP",g_temp,g_source,g_ice,g_template);
            }
          }
          else if ((uVar15 == 0x14 && cVar10 == -0x3c) && (uVar26 != 0)) {
            g_ice = (uint)(*pbVar16 != 0);
            log_num(&DAT_0001023e,uVar18,0x14,0xc4,g_ice);
            publish();
          }
          else if ((uVar15 == 0x28 && cVar10 == '\x01') && (uVar26 != 0)) {
            log_text("SOURCE_TEXT",pbVar16,uVar26);
            uVar15 = 0;
            do {
              if ((uVar15 + 2 < uVar26) &&
                 (uVar25 = (int)&local_514 + uVar15 + iVar12 + 0x514, *(char *)(uVar25 + 5) == 'A'))
              {
                bVar30 = *(char *)(uVar25 + 6) == 'U';
                if (bVar30) {
                  uVar25 = (uint)*(byte *)(uVar25 + 7);
                }
                if (!bVar30 || uVar25 != 0x58) goto LAB_000211ec;
                g_source = 5;
                goto LAB_00021630;
              }
LAB_000211ec:
              uVar25 = uVar15 + 1;
              if (uVar25 < uVar26) {
                uVar23 = (int)&local_514 + uVar15 + iVar12 + 0x514;
                cVar10 = *(char *)(uVar23 + 5);
                if (cVar10 == 'M') {
                  if (*(char *)(uVar23 + 6) == 'W') {
                    g_source = 2;
                    goto LAB_00021630;
                  }
                  goto LAB_0002124c;
                }
                if (cVar10 == 'L') {
                  if (*(char *)(uVar23 + 6) != 'W') goto LAB_0002124c;
                  g_source = 3;
                }
                else {
                  if (cVar10 == 'F') {
                    uVar23 = (uint)*(byte *)(uVar23 + 6);
                  }
                  if (cVar10 != 'F' || uVar23 != 0x4d) goto LAB_0002124c;
                  g_source = 1;
                }
LAB_00021630:
                log_num("SOURCE_TEXT_MAP",uVar18,0x28,1,g_source);
                publish();
                log_num("PUBLISH_SOURCE",g_temp,g_source,g_ice,g_template);
                goto LAB_0002168c;
              }
LAB_0002124c:
              if ((uVar15 + 2 < uVar26) &&
                 (uVar23 = (int)&local_514 + uVar15 + iVar12 + 0x514, *(char *)(uVar23 + 5) == 'C'))
              {
                bVar30 = *(char *)(uVar23 + 6) == 'D';
                if (bVar30) {
                  uVar23 = (uint)*(byte *)(uVar23 + 7);
                }
                if (!bVar30 || uVar23 != 0x43) goto LAB_00021278;
LAB_00021604:
                g_source = 4;
                goto LAB_00021630;
              }
LAB_00021278:
              if (uVar25 < uVar26) {
                uVar15 = (int)&local_514 + uVar15 + iVar12 + 0x514;
                bVar30 = *(char *)(uVar15 + 5) == 'C';
                if (bVar30) {
                  uVar15 = (uint)*(byte *)(uVar15 + 6);
                }
                if (bVar30 && uVar15 == 0x44) goto LAB_00021604;
              }
              uVar15 = uVar25;
            } while (uVar26 != uVar25);
            log_num("SOURCE_TEXT_UNKNOWN",uVar18,0x28,1,uVar26);
          }
          else if (((uVar15 == 0x3c || uVar15 == 100) && cVar10 == '\x01') && (uVar26 != 0)) {
            pcVar13 = "SOURCE_STATUS";
            if (uVar15 == 0x3c) {
              pcVar13 = "SOURCE_MODE";
            }
            log_text(pcVar13,pbVar16,uVar26);
            uVar25 = 0;
            bVar5 = false;
            bVar4 = false;
            bVar30 = false;
            bVar6 = false;
            bVar7 = false;
            do {
              iVar9 = uVar25 + iVar12;
              if (uVar25 + 2 < uVar26) {
                cVar10 = *(char *)((int)&local_510 + iVar9 + 0x515);
                if ((byte)(cVar10 + 0x9fU) < 0x1a) {
                  cVar10 = cVar10 + -0x20;
                }
                if (cVar10 == 'A') {
                  cVar10 = *(char *)((int)&local_510 + iVar9 + 0x516);
                  if ((byte)(cVar10 + 0x9fU) < 0x1a) {
                    cVar10 = cVar10 + -0x20;
                  }
                  if (cVar10 == 'U') {
                    cVar10 = *(char *)((int)&local_510 + iVar9 + 0x517);
                    if ((byte)(cVar10 + 0x9fU) < 0x1a) {
                      cVar10 = cVar10 + -0x20;
                    }
                    if (cVar10 == 'X') {
                      bVar30 = true;
                    }
                  }
                }
              }
              uVar25 = uVar25 + 1;
              if (uVar25 < uVar26) {
                bVar11 = *(byte *)((int)&local_510 + iVar9 + 0x515);
                if ((byte)(bVar11 + 0x9f) < 0x1a) {
                  bVar11 = bVar11 - 0x20;
                }
                if (bVar11 < 0x4c) {
                  if (bVar11 == 0x43) {
                    cVar10 = *(char *)((int)&local_510 + iVar9 + 0x516);
                    if ((byte)(cVar10 + 0x9fU) < 0x1a) {
                      cVar10 = cVar10 + -0x20;
                    }
                    if (cVar10 == 'D') {
                      bVar6 = true;
                    }
                  }
                  else if (bVar11 == 0x46) {
                    cVar10 = *(char *)((int)&local_510 + iVar9 + 0x516);
                    if ((byte)(cVar10 + 0x9fU) < 0x1a) {
                      cVar10 = cVar10 + -0x20;
                    }
                    if (cVar10 == 'M') {
                      bVar4 = true;
                    }
                  }
                }
                else if (bVar11 == 0x4c) {
                  cVar10 = *(char *)((int)&local_510 + iVar9 + 0x516);
                  if ((byte)(cVar10 + 0x9fU) < 0x1a) {
                    cVar10 = cVar10 + -0x20;
                  }
                  if (cVar10 == 'W') {
                    bVar7 = true;
                  }
                }
                else if (bVar11 == 0x4d) {
                  cVar10 = *(char *)((int)&local_510 + iVar9 + 0x516);
                  if ((byte)(cVar10 + 0x9fU) < 0x1a) {
                    cVar10 = cVar10 + -0x20;
                  }
                  if (cVar10 == 'W') {
                    bVar5 = true;
                  }
                }
              }
            } while (uVar26 != uVar25);
            uVar25 = 5;
            if ((((bVar30) || (uVar25 = 1, bVar4)) || (uVar25 = 2, bVar5)) ||
               ((uVar25 = 3, bVar7 || (uVar25 = 4, bVar6)))) {
              g_source = uVar25;
              log_num("SOURCE_DYNAMIC_MAP",uVar18,uVar15,1,uVar25);
              publish();
              pcVar13 = "PUBLISH_SOURCE";
              uVar18 = g_temp;
              uVar15 = g_source;
              uVar25 = g_ice;
              uVar26 = g_template;
            }
            else {
              pcVar13 = "SOURCE_DYNAMIC_IGNORE";
              uVar25 = 1;
            }
            log_num(pcVar13,uVar18,uVar15,uVar25,uVar26);
          }
          else if ((uVar15 == 0x2c && cVar10 == '\x02') && (uVar26 != 0)) {
            uVar15 = (uint)*pbVar16;
            if (1 < uVar26) {
              uVar15 = (uint)CONCAT11(*(undefined1 *)((int)&local_510 + uVar8 + 2),*pbVar16);
            }
            uVar26 = 4;
            if (1 < uVar15 - 200) {
              if (uVar15 != 0xcb) break;
              uVar26 = 5;
            }
            pcVar13 = "SOURCE_PROP";
            uVar19 = 0x2c;
            uVar21 = 2;
            g_source = uVar26;
            goto LAB_00021074;
          }
          break;
        case 0xe:
          if ((int)uVar24 < 6) {
            pcVar13 = "TRUNC_SETIDX_HDR";
            goto LAB_0002173c;
          }
          uVar18 = (uint)*(byte *)((int)&local_510 + uVar8 + 1);
          uVar29 = uVar18 + 6;
          if (uVar29 <= uVar24) {
            uVar15 = (uint)*(byte *)((int)&local_514 + uVar8 + 1);
            uVar20 = *(undefined1 *)((int)&local_514 + uVar8 + 3);
            cVar10 = *(char *)((int)&local_514 + uVar8 + 2);
            cVar3 = *(char *)((int)&local_510 + uVar8);
            g_viewer = uVar15;
            log_num(&DAT_00010248,uVar8,0xe,uVar29,bVar11 >> 7);
            log_num("SETIDX",uVar15,cVar10,uVar20,cVar3);
            if ((uVar18 == 0) || (cVar10 != ',' || cVar3 != '\x02')) break;
            bVar11 = *(byte *)((int)&local_510 + uVar8 + 2);
            uVar26 = (uint)bVar11;
            if (1 < uVar18) {
              uVar26 = (uint)CONCAT11(*(undefined1 *)((int)&local_510 + uVar8 + 3),bVar11);
            }
            uVar18 = 4;
            if (uVar26 - 200 < 2) {
LAB_00020e9c:
              g_source = uVar18;
              publish();
            }
            else if (uVar26 == 0xcb) {
              uVar18 = 5;
              goto LAB_00020e9c;
            }
            log_num("SOURCE_GFX",uVar15,uVar20,2,uVar26);
            break;
          }
          pcVar13 = "TRUNC_SETIDX_DATA";
          uVar28 = uVar8;
          uVar15 = uVar24;
          goto LAB_00020804;
        case 0x11:
          if ((int)uVar24 < 2) {
            pcVar13 = "TRUNC_UPDATE";
            goto LAB_0002173c;
          }
          g_viewer = (uint)*(byte *)((int)&local_514 + uVar8 + 1);
          uVar29 = 2;
          log_num(&DAT_00010248,uVar8,0x11,2,bVar11 >> 7);
          pcVar13 = "UPDATE_SCREEN";
          uVar19 = 0;
          uVar21 = 0;
          uVar15 = 0;
          uVar18 = g_viewer;
LAB_00021074:
          log_num(pcVar13,uVar18,uVar19,uVar21,uVar15);
          publish();
          break;
        case 0x12:
          if ((int)uVar24 < 3) {
            pcVar13 = "TRUNC_RESET";
            goto LAB_0002173c;
          }
          log_num(&DAT_00010248,uVar8,0x12,3,bVar11 >> 7);
          uVar18 = (uint)*(byte *)((int)&local_514 + uVar8 + 2);
          pcVar13 = "RESET_PROPERTIES";
          uVar29 = 3;
          uVar15 = (uint)*(byte *)((int)&local_514 + uVar8 + 1);
LAB_00020dd0:
          uVar20 = 0;
LAB_00020f04:
          log_num(pcVar13,uVar15,uVar18,uVar20,0);
          break;
        case 0x13:
          if ((int)uVar24 < 4) {
            pcVar13 = "TRUNC_RESETIDX";
            goto LAB_0002173c;
          }
          log_num(&DAT_00010248,uVar8,0x13,4,bVar11 >> 7);
          uVar15 = (uint)*(byte *)((int)&local_514 + uVar8 + 1);
          uVar18 = (uint)*(byte *)((int)&local_514 + uVar8 + 2);
          uVar20 = *(undefined1 *)((int)&local_514 + uVar8 + 3);
          pcVar13 = "RESET_INDEXED";
          uVar29 = 4;
          goto LAB_00020f04;
        }
LAB_0002168c:
        if ((int)uVar24 < (int)uVar29) {
          pcVar13 = "BAD_METHOD_SIZE";
          uVar18 = uVar24;
          uVar15 = uVar8;
LAB_00021750:
          log_num(pcVar13,uVar28,uVar29,uVar18,uVar15);
          break;
        }
        uVar8 = uVar29 + uVar8;
      } while ((int)uVar8 < 10);
LAB_00021754:
      publish();
    }
    software_interrupt(0x900066);
  } while( true );
}



/* ===== 0x21884 | log_line | ev_7db67ec460bce2663d01e739510c8ebf504498c0d00d91b2e5b46726beb589bd ===== */

/* WARNING: Removing unreachable block (ram,0x000218a8) */

undefined8 log_line(int param_1)

{
  char *pcVar1;
  char *pcVar2;
  char *pcVar3;
  
  pcVar1 = (char *)0x441;
  software_interrupt(0x900005);
  pcVar3 = (char *)0x0;
  do {
    pcVar2 = pcVar3;
    pcVar3 = pcVar2 + 1;
  } while (pcVar2[param_1] != '\0');
  for (pcVar3 = (char *)0x0; pcVar3 < pcVar2; pcVar3 = "/tmp/cfbus" + (int)pcVar3) {
    pcVar1 = pcVar3 + param_1;
    software_interrupt(0x900004);
  }
  pcVar3 = (char *)0x0;
  while (pcVar3 == (char *)0x0) {
    pcVar1 = "\n";
    software_interrupt(0x900004);
    pcVar3 = "/tmp/cfbus";
  }
  software_interrupt(0x900006);
  return CONCAT44(pcVar1,"/tmp/cfbus");
}



/* ===== 0x21930 | publish | ev_688ffe985b5dd750f5a8b9b561cdd547ac3d31a9fceb5e903f7e60cf9410e12f ===== */

undefined8 publish(void)

{
  uint uVar1;
  int iVar2;
  char *pcVar3;
  char *pcVar4;
  int iVar5;
  char *pcVar6;
  char *pcVar7;
  char cVar8;
  char local_b0 [160];
  
  builtin_strncpy(local_b0,"src=",4);
  if (g_source - 1U < 5) {
    pcVar4 = (&PTR_DAT_00010514)[g_source - 1U];
  }
  else {
    pcVar4 = "unknown";
  }
  cVar8 = *pcVar4;
  if (cVar8 == '\0') {
    iVar5 = 4;
  }
  else {
    iVar5 = 0;
    do {
      iVar2 = iVar5;
      local_b0[iVar2 + 4] = cVar8;
      if (0x9d < iVar2 + 4U) break;
      cVar8 = pcVar4[iVar2 + 1];
      iVar5 = iVar2 + 1;
    } while (cVar8 != '\0');
    iVar5 = 0x9f;
    if (0x9d < iVar2 + 4U) goto LAB_00021a04;
    iVar5 = iVar2 + 5;
  }
  pcVar4 = "\ntemp=";
  uVar1 = 0x9e - iVar5;
  cVar8 = '\n';
  if (4 < uVar1) {
    uVar1 = 5;
  }
  iVar2 = uVar1 + 1;
  do {
    pcVar4 = pcVar4 + 1;
    local_b0[iVar5] = cVar8;
    cVar8 = *pcVar4;
    iVar2 = iVar2 + -1;
    iVar5 = iVar5 + 1;
  } while (iVar2 != 0);
LAB_00021a04:
  iVar5 = dec_append(local_b0,iVar5,0xa0,g_temp);
  if (iVar5 < 0x9f) {
    pcVar4 = "\nice=";
    uVar1 = 0x9e - iVar5;
    cVar8 = '\n';
    if (3 < uVar1) {
      uVar1 = 4;
    }
    iVar2 = uVar1 + 1;
    do {
      pcVar4 = pcVar4 + 1;
      local_b0[iVar5] = cVar8;
      cVar8 = *pcVar4;
      iVar2 = iVar2 + -1;
      iVar5 = iVar5 + 1;
    } while (iVar2 != 0);
  }
  iVar5 = dec_append(local_b0,iVar5,0xa0,g_ice);
  if (iVar5 < 0x9f) {
    pcVar4 = "\nviewer=";
    uVar1 = 0x9e - iVar5;
    cVar8 = '\n';
    if (6 < uVar1) {
      uVar1 = 7;
    }
    iVar2 = uVar1 + 1;
    do {
      pcVar4 = pcVar4 + 1;
      local_b0[iVar5] = cVar8;
      cVar8 = *pcVar4;
      iVar2 = iVar2 + -1;
      iVar5 = iVar5 + 1;
    } while (iVar2 != 0);
  }
  iVar5 = dec_append(local_b0,iVar5,0xa0,g_viewer);
  if (iVar5 < 0x9f) {
    pcVar4 = "\ntemplate=";
    uVar1 = 0x9e - iVar5;
    cVar8 = '\n';
    if (8 < uVar1) {
      uVar1 = 9;
    }
    iVar2 = uVar1 + 1;
    do {
      pcVar4 = pcVar4 + 1;
      local_b0[iVar5] = cVar8;
      cVar8 = *pcVar4;
      iVar2 = iVar2 + -1;
      iVar5 = iVar5 + 1;
    } while (iVar2 != 0);
  }
  pcVar3 = (char *)dec_append(local_b0,iVar5,0xa0,g_template);
  pcVar7 = (char *)0x0;
  pcVar4 = pcVar3;
  if ((int)pcVar3 < 0x9f) {
    pcVar4 = (char *)0xa;
  }
  pcVar6 = (char *)0x241;
  if ((int)pcVar3 < 0x9f) {
    local_b0[(int)pcVar3] = (char)pcVar4;
    pcVar3 = pcVar3 + 1;
  }
  local_b0[(int)pcVar3] = '\0';
  software_interrupt(0x900005);
  for (; pcVar7 < pcVar3; pcVar7 = "/tmp/cfstate" + (int)pcVar7) {
    pcVar6 = local_b0 + (int)pcVar7;
    software_interrupt(0x900004);
  }
  software_interrupt(0x900006);
  return CONCAT44(pcVar6,"/tmp/cfstate");
}



/* ===== 0x21bbc | log_num | ev_a899e02ea2a12c37b0ec8c6c89e8384681692626c32781cd4bcd419b24d5fdf8 ===== */

void log_num(char *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
            undefined4 param_5)

{
  uint uVar1;
  int iVar2;
  uint uVar3;
  int iVar4;
  uint uVar5;
  char *pcVar6;
  char cVar7;
  char local_c0 [160];
  
  cVar7 = *param_1;
  if (cVar7 == '\0') {
    uVar3 = 0;
  }
  else {
    uVar3 = 0;
    do {
      uVar1 = uVar3;
      local_c0[uVar1] = cVar7;
      if (0x9d < uVar1) break;
      cVar7 = param_1[uVar1 + 1];
      uVar3 = uVar1 + 1;
    } while (cVar7 != '\0');
    uVar5 = 0x9f;
    uVar3 = uVar1 + 1;
    if (0x9d < uVar1) goto LAB_00021c58;
  }
  uVar5 = uVar3;
  uVar3 = 0x9e - uVar5;
  cVar7 = ' ';
  if (1 < uVar3) {
    uVar3 = 2;
  }
  iVar2 = uVar3 + 1;
  pcVar6 = " a=";
  do {
    pcVar6 = pcVar6 + 1;
    local_c0[uVar5] = cVar7;
    cVar7 = *pcVar6;
    iVar2 = iVar2 + -1;
    uVar5 = uVar5 + 1;
  } while (iVar2 != 0);
LAB_00021c58:
  iVar2 = dec_append(local_c0,uVar5,0xa0,param_2);
  if (iVar2 < 0x9f) {
    pcVar6 = " b=";
    uVar3 = 0x9e - iVar2;
    cVar7 = ' ';
    if (1 < uVar3) {
      uVar3 = 2;
    }
    iVar4 = uVar3 + 1;
    do {
      pcVar6 = pcVar6 + 1;
      local_c0[iVar2] = cVar7;
      cVar7 = *pcVar6;
      iVar4 = iVar4 + -1;
      iVar2 = iVar2 + 1;
    } while (iVar4 != 0);
  }
  iVar2 = dec_append(local_c0,iVar2,0xa0,param_3);
  if (iVar2 < 0x9f) {
    pcVar6 = " c=";
    uVar3 = 0x9e - iVar2;
    cVar7 = ' ';
    if (1 < uVar3) {
      uVar3 = 2;
    }
    iVar4 = uVar3 + 1;
    do {
      pcVar6 = pcVar6 + 1;
      local_c0[iVar2] = cVar7;
      cVar7 = *pcVar6;
      iVar4 = iVar4 + -1;
      iVar2 = iVar2 + 1;
    } while (iVar4 != 0);
  }
  iVar2 = dec_append(local_c0,iVar2,0xa0,param_4);
  if (iVar2 < 0x9f) {
    pcVar6 = " d=";
    uVar3 = 0x9e - iVar2;
    cVar7 = ' ';
    if (1 < uVar3) {
      uVar3 = 2;
    }
    iVar4 = uVar3 + 1;
    do {
      pcVar6 = pcVar6 + 1;
      local_c0[iVar2] = cVar7;
      cVar7 = *pcVar6;
      iVar4 = iVar4 + -1;
      iVar2 = iVar2 + 1;
    } while (iVar4 != 0);
  }
  iVar2 = dec_append(local_c0,iVar2,0xa0,param_5);
  local_c0[iVar2] = '\0';
  log_line(local_c0);
  return;
}



/* ===== 0x21d80 | _start | ev_05805acdf1ff6bb31677fd16853afe3b15f273612b920ef68eab6cbd3198bf08 ===== */

/* WARNING: Removing unreachable block (ram,0x000207dc) */
/* WARNING: Removing unreachable block (ram,0x00020628) */
/* WARNING: Removing unreachable block (ram,0x000205e0) */
/* WARNING: Removing unreachable block (ram,0x00020674) */
/* WARNING: Removing unreachable block (ram,0x000207d8) */
/* WARNING: Removing unreachable block (ram,0x00021790) */
/* WARNING: Removing unreachable block (ram,0x00020998) */
/* WARNING: Removing unreachable block (ram,0x000209c8) */
/* WARNING: Removing unreachable block (ram,0x000209dc) */
/* WARNING: Removing unreachable block (ram,0x000209ec) */
/* WARNING: Removing unreachable block (ram,0x000209f0) */
/* WARNING: Removing unreachable block (ram,0x000209f8) */
/* WARNING: Removing unreachable block (ram,0x00020a0c) */

void _start(void)

{
  byte bVar1;
  byte bVar2;
  char cVar3;
  bool bVar4;
  bool bVar5;
  bool bVar6;
  bool bVar7;
  uint uVar8;
  int iVar9;
  char cVar10;
  byte bVar11;
  int iVar12;
  char *pcVar13;
  undefined1 *puVar14;
  uint uVar15;
  byte *pbVar16;
  undefined1 uVar17;
  uint uVar18;
  undefined4 uVar19;
  undefined1 uVar20;
  undefined4 uVar21;
  undefined1 *puVar22;
  uint uVar23;
  uint uVar24;
  uint uVar25;
  uint uVar26;
  char *pcVar27;
  uint uVar28;
  uint uVar29;
  bool bVar30;
  uint uStack_588;
  undefined2 uStack_582;
  char acStack_580 [17];
  undefined1 uStack_56f;
  undefined4 uStack_514;
  undefined4 uStack_510;
  undefined4 uStack_50c;
  undefined1 uStack_111;
  undefined1 uStack_110;
  undefined1 uStack_10f;
  undefined1 uStack_10e;
  undefined1 uStack_10d;
  undefined4 *apuStack_10c [65];
  
  iVar12 = 0;
  do {
    *(undefined1 *)((int)&uStack_582 + iVar12) = 0;
    iVar12 = iVar12 + 1;
  } while (iVar12 != 0x6e);
  iVar12 = 0;
  uStack_582 = 1;
  do {
    acStack_580[iVar12] = "/var/run/PAC09900"[iVar12];
    iVar12 = iVar12 + 1;
  } while (iVar12 != 0x11);
  uStack_56f = 0;
  software_interrupt(0x90000a);
  software_interrupt(0x90000a);
  software_interrupt(0x90000a);
  log_line("--- RUNTIME0310 PAC FOCUS ---");
  publish();
  uStack_50c = 0;
  uStack_510 = (undefined2 *)0x5;
  uStack_514 = 1;
  software_interrupt(0x900066);
  log_num("PAC_SERVER_CREATE",1,5,0,0);
  uStack_514 = 1;
  uStack_50c = 0x14;
  software_interrupt(0x900066);
  uStack_510 = &uStack_582;
  log_num("PAC_BIND",2,0x14,0,0);
  uStack_514 = 1;
  uStack_510 = (undefined2 *)0x4;
  software_interrupt(0x900066);
  log_num("PAC_LISTEN",4,4,0,0);
  pcVar13 = (char *)0x241;
  iVar12 = 0x1b6;
  software_interrupt(0x900005);
  for (pcVar27 = (char *)0x0; pcVar27 < (char *)0x6; pcVar27 = "/tmp/cfready" + (int)pcVar27) {
    pcVar13 = "READY\n" + (int)pcVar27;
    iVar12 = 6 - (int)pcVar27;
    software_interrupt(0x900004);
  }
  software_interrupt(0x900006);
  log_line("PAC_READY",pcVar13,iVar12);
  apuStack_10c[2] = (undefined4 *)0x0;
  apuStack_10c[1] = (undefined4 *)0x0;
  apuStack_10c[0] = (undefined4 *)0x1;
  software_interrupt(0x900066);
  log_num("PAC_ACCEPT_OK",5,0,0,0);
  software_interrupt(0x900066);
  do {
    apuStack_10c[3] = (undefined4 *)0x0;
    apuStack_10c[2] = (undefined4 *)0x400;
    uStack_588 = 2;
    uVar8 = uStack_514 >> 0x10 & 0x7f;
    if (((0x13 < uVar8) || ((1 << uVar8 & 0xe687eU) == 0)) && (uVar8 != 0x7f)) {
      uVar8 = (uStack_514 & 0x7f) - 1;
      if (((0x12 < uVar8) || (uStack_588 = 0, (0x7343fU >> (uVar8 & 0xff) & 1) == 0)) &&
         (uStack_588 = 0, (uStack_514 & 0x7f) != 0x7f)) {
        uStack_588 = 2;
      }
    }
    g_rx = g_rx + 1;
    apuStack_10c[0] = (undefined4 *)0x235852;
    apuStack_10c[1] = &uStack_514;
    iVar12 = dec_append(apuStack_10c,3,0x104);
    if (iVar12 < 0x103) {
      uVar8 = 0x102 - iVar12;
      uVar20 = 0x20;
      if (1 < uVar8) {
        uVar8 = 2;
      }
      iVar9 = uVar8 + 1;
      puVar14 = &DAT_00010208;
      do {
        *(undefined1 *)((int)apuStack_10c + iVar12) = uVar20;
        uVar20 = *puVar14;
        iVar9 = iVar9 + -1;
        iVar12 = iVar12 + 1;
        puVar14 = puVar14 + 1;
      } while (iVar9 != 0);
    }
    iVar12 = dec_append(apuStack_10c,iVar12,0x104,10);
    if (iVar12 < 0x103) {
      uVar8 = 0x102 - iVar12;
      uVar20 = 0x20;
      if (4 < uVar8) {
        uVar8 = 5;
      }
      iVar9 = uVar8 + 1;
      puVar14 = &DAT_00010350;
      do {
        *(undefined1 *)((int)apuStack_10c + iVar12) = uVar20;
        uVar20 = *puVar14;
        iVar9 = iVar9 + -1;
        iVar12 = iVar12 + 1;
        puVar14 = puVar14 + 1;
      } while (iVar9 != 0);
    }
    iVar12 = dec_append(apuStack_10c,iVar12,0x104,uStack_588);
    if (iVar12 < 0x103) {
      *(undefined1 *)((int)apuStack_10c + iVar12) = 0x3a;
      iVar12 = iVar12 + 1;
    }
    iVar9 = 0;
    do {
      iVar12 = hexbyte_append(apuStack_10c,iVar12,0x104,*(undefined1 *)((int)&uStack_514 + iVar9));
      iVar9 = iVar9 + 1;
    } while (iVar9 != 10);
    *(undefined1 *)((int)apuStack_10c + iVar12) = 0;
    log_line(apuStack_10c);
    uVar8 = uStack_588;
    if (uStack_588 < 10) {
      do {
        uVar18 = 10;
        bVar11 = *(byte *)((int)&uStack_514 + uVar8);
        uVar24 = 10 - uVar8;
        uVar28 = bVar11 & 0x7f;
        if (0x12 < uVar28 - 1) {
switchD_00020a60_caseD_2:
          pcVar13 = "UNHANDLED_METHOD";
          uVar15 = (uint)bVar11;
          uVar29 = uVar8;
          uVar18 = uVar24;
LAB_00020804:
          log_num(pcVar13,uVar28,uVar15,uVar29,uVar18);
          break;
        }
        iVar12 = uVar8 - 0x514;
        switch(uVar28) {
        case 1:
          if (3 < (int)uVar24) {
            uVar20 = *(undefined1 *)((int)&uStack_514 + uVar8 + 1);
            bVar1 = *(byte *)((int)&uStack_514 + uVar8 + 2);
            bVar2 = *(byte *)((int)&uStack_514 + uVar8 + 3);
            log_num(&DAT_00010248,uVar8,1,4,bVar11 >> 7);
            log_num("CONNECT REQUEST",uVar20,bVar1,bVar2,uStack_588);
            uVar20 = 2;
            uStack_111 = 2;
            if (bVar2 != 0) {
              uVar20 = 1;
            }
            if (1 < bVar1) {
              uVar20 = 2;
            }
            uVar17 = uVar20;
            if (1 < bVar2) {
              uVar17 = 2;
            }
            uVar19 = 2;
            if (2 < bVar1) {
              uVar17 = uVar20;
            }
            puVar14 = &uStack_110;
            if (uStack_588 == 2) {
              uStack_10f = 2;
              uStack_111 = (undefined1)uStack_514;
              uVar19 = 4;
              uStack_110 = uStack_514._1_1_;
              puVar14 = &uStack_10e;
            }
            *puVar14 = uVar17;
            apuStack_10c[3] = (undefined4 *)0x4000;
            apuStack_10c[0] = (undefined4 *)0x5;
            software_interrupt(0x900066);
            apuStack_10c[1] = (undefined4 *)&uStack_111;
            apuStack_10c[2] = (undefined4 *)uVar19;
            log_num("CONNECT_RESPONSE",uVar17,9,uVar19,uStack_588);
            log_tx("TX CONNECT_RESPONSE",&uStack_111,uVar19,9);
            uVar29 = 4;
            break;
          }
          pcVar13 = "TRUNC_CONNECT";
LAB_0002173c:
          uVar15 = 0;
          uVar18 = 0;
          uVar28 = uVar8;
          uVar29 = uVar24;
          goto LAB_00021750;
        default:
          goto switchD_00020a60_caseD_2;
        case 3:
          log_num(&DAT_00010248,uVar8,3,1,bVar11 >> 7);
          log_num("DISCONNECT_REQUEST",uStack_588,10,0,0);
          apuStack_10c[2] = (undefined4 *)0x1;
          uStack_111 = 4;
          if (uStack_588 == 2) {
            uStack_10f = 4;
            apuStack_10c[2] = (undefined4 *)0x3;
            uStack_110 = uStack_514._1_1_;
            uStack_111 = (undefined1)uStack_514;
          }
          apuStack_10c[3] = (undefined4 *)0x4000;
          apuStack_10c[1] = (undefined4 *)&uStack_111;
          apuStack_10c[0] = (undefined4 *)0x5;
          software_interrupt(0x900066);
          log_tx("TX DISCONNECT_RESPONSE",apuStack_10c[1],apuStack_10c[2],9);
          uVar29 = 1;
          break;
        case 5:
          if ((int)uVar24 < 2) {
            pcVar13 = "TRUNC_FULLMODE";
            goto LAB_0002173c;
          }
          uVar20 = *(undefined1 *)((int)&uStack_514 + uVar8 + 1);
          log_num(&DAT_00010248,uVar8,5,2,bVar11 >> 7);
          log_num("REQUEST_FULL_MODE",uVar20,uStack_588,10,0);
          if (uStack_588 == 2) {
            apuStack_10c[2] = (undefined4 *)0x5;
            uStack_10f = 0x7f;
            uStack_110 = uStack_514._1_1_;
            puVar14 = &uStack_10e;
            puVar22 = &uStack_10d;
            uStack_111 = (undefined1)uStack_514;
          }
          else {
            apuStack_10c[2] = (undefined4 *)0x3;
            uStack_111 = 0x7f;
            puVar14 = &uStack_110;
            puVar22 = &uStack_10f;
          }
          *puVar22 = 0;
          apuStack_10c[1] = (undefined4 *)&uStack_111;
          *puVar14 = 5;
          apuStack_10c[3] = (undefined4 *)0x4000;
          apuStack_10c[0] = (undefined4 *)0x5;
          software_interrupt(0x900066);
          log_tx("TX F_REPLY_FULL_MODE",apuStack_10c[1],apuStack_10c[2],9);
          log_num("FULL_MODE_REPLY",uVar20,9,uStack_588,0);
          uVar29 = 2;
          break;
        case 6:
          uVar29 = 1;
          log_num(&DAT_00010248,uVar8,6,1,bVar11 >> 7);
          pcVar13 = "RELEASE_FULL_MODE";
          uVar15 = uStack_588;
          goto LAB_00020dd0;
        case 0xb:
          if ((int)uVar24 < 3) {
            pcVar13 = "TRUNC_TEMPLATE";
            goto LAB_0002173c;
          }
          uVar15 = (uint)*(byte *)((int)&uStack_514 + uVar8 + 1);
          uVar18 = (uint)*(byte *)((int)&uStack_514 + uVar8 + 2);
          uVar29 = 3;
          log_num(&DAT_00010248,uVar8,0xb,3,bVar11 >> 7);
          pcVar13 = "TEMPLATE";
          g_viewer = uVar15;
          g_template = uVar18;
          goto LAB_00020dd0;
        case 0xd:
          if ((int)uVar24 < 5) {
            pcVar13 = "TRUNC_SETPROP_HDR";
            goto LAB_0002173c;
          }
          uVar26 = (uint)*(byte *)((int)&uStack_510 + uVar8);
          uVar29 = uVar26 + 5;
          if (uVar24 < uVar29) {
            log_num("TRUNC_SETPROP_DATA",uVar8,uVar24,uVar29,uVar26);
            goto LAB_00021754;
          }
          uVar18 = (uint)*(byte *)((int)&uStack_514 + uVar8 + 1);
          uVar15 = (uint)*(byte *)((int)&uStack_514 + uVar8 + 2);
          cVar10 = *(char *)((int)&uStack_514 + uVar8 + 3);
          g_viewer = uVar18;
          log_num(&DAT_00010248,uVar8,0xd,uVar29,bVar11 >> 7);
          log_num("SETPROP",uVar18,uVar15,cVar10,uVar26);
          pbVar16 = (byte *)((int)&uStack_510 + uVar8 + 1);
          if ((uVar15 == 0x14 && cVar10 == '\x01') && (uVar26 != 0)) {
            if (*pbVar16 == 0x2d) {
              iVar12 = -1;
              uVar15 = 1;
            }
            else if (*pbVar16 == 0x2b) {
              uVar15 = 1;
              iVar12 = 1;
            }
            else {
              iVar12 = 1;
              uVar15 = 0;
            }
            if (uVar15 < uVar26) {
              iVar9 = 0;
              uVar25 = 0;
              while (uVar23 = (uint)*(byte *)((int)&uStack_510 + uVar25 + uVar15 + uVar8 + 1),
                    0xfffffff5 < uVar23 - 0x3a) {
                iVar9 = uVar23 + iVar9 * 10 + -0x30;
                if ((uVar26 <= uVar15 + 1 + uVar25) ||
                   (bVar30 = 2 < uVar25, uVar25 = uVar25 + 1, bVar30)) goto LAB_000215a0;
              }
              if (uVar25 == 0) break;
LAB_000215a0:
              g_temp = iVar9 * iVar12;
              log_num(&DAT_00010462,uVar18,0x14,1,g_temp);
              publish();
              log_num("PUBLISH_TEMP",g_temp,g_source,g_ice,g_template);
            }
          }
          else if ((uVar15 == 0x14 && cVar10 == -0x3c) && (uVar26 != 0)) {
            g_ice = (uint)(*pbVar16 != 0);
            log_num(&DAT_0001023e,uVar18,0x14,0xc4,g_ice);
            publish();
          }
          else if ((uVar15 == 0x28 && cVar10 == '\x01') && (uVar26 != 0)) {
            log_text("SOURCE_TEXT",pbVar16,uVar26);
            uVar15 = 0;
            do {
              if ((uVar15 + 2 < uVar26) &&
                 (uVar25 = (int)&uStack_514 + uVar15 + iVar12 + 0x514, *(char *)(uVar25 + 5) == 'A')
                 ) {
                bVar30 = *(char *)(uVar25 + 6) == 'U';
                if (bVar30) {
                  uVar25 = (uint)*(byte *)(uVar25 + 7);
                }
                if (!bVar30 || uVar25 != 0x58) goto LAB_000211ec;
                g_source = 5;
                goto LAB_00021630;
              }
LAB_000211ec:
              uVar25 = uVar15 + 1;
              if (uVar25 < uVar26) {
                uVar23 = (int)&uStack_514 + uVar15 + iVar12 + 0x514;
                cVar10 = *(char *)(uVar23 + 5);
                if (cVar10 == 'M') {
                  if (*(char *)(uVar23 + 6) == 'W') {
                    g_source = 2;
                    goto LAB_00021630;
                  }
                  goto LAB_0002124c;
                }
                if (cVar10 == 'L') {
                  if (*(char *)(uVar23 + 6) != 'W') goto LAB_0002124c;
                  g_source = 3;
                }
                else {
                  if (cVar10 == 'F') {
                    uVar23 = (uint)*(byte *)(uVar23 + 6);
                  }
                  if (cVar10 != 'F' || uVar23 != 0x4d) goto LAB_0002124c;
                  g_source = 1;
                }
LAB_00021630:
                log_num("SOURCE_TEXT_MAP",uVar18,0x28,1,g_source);
                publish();
                log_num("PUBLISH_SOURCE",g_temp,g_source,g_ice,g_template);
                goto LAB_0002168c;
              }
LAB_0002124c:
              if ((uVar15 + 2 < uVar26) &&
                 (uVar23 = (int)&uStack_514 + uVar15 + iVar12 + 0x514, *(char *)(uVar23 + 5) == 'C')
                 ) {
                bVar30 = *(char *)(uVar23 + 6) == 'D';
                if (bVar30) {
                  uVar23 = (uint)*(byte *)(uVar23 + 7);
                }
                if (!bVar30 || uVar23 != 0x43) goto LAB_00021278;
LAB_00021604:
                g_source = 4;
                goto LAB_00021630;
              }
LAB_00021278:
              if (uVar25 < uVar26) {
                uVar15 = (int)&uStack_514 + uVar15 + iVar12 + 0x514;
                bVar30 = *(char *)(uVar15 + 5) == 'C';
                if (bVar30) {
                  uVar15 = (uint)*(byte *)(uVar15 + 6);
                }
                if (bVar30 && uVar15 == 0x44) goto LAB_00021604;
              }
              uVar15 = uVar25;
            } while (uVar26 != uVar25);
            log_num("SOURCE_TEXT_UNKNOWN",uVar18,0x28,1,uVar26);
          }
          else if (((uVar15 == 0x3c || uVar15 == 100) && cVar10 == '\x01') && (uVar26 != 0)) {
            pcVar13 = "SOURCE_STATUS";
            if (uVar15 == 0x3c) {
              pcVar13 = "SOURCE_MODE";
            }
            log_text(pcVar13,pbVar16,uVar26);
            uVar25 = 0;
            bVar5 = false;
            bVar4 = false;
            bVar30 = false;
            bVar6 = false;
            bVar7 = false;
            do {
              iVar9 = uVar25 + iVar12;
              if (uVar25 + 2 < uVar26) {
                cVar10 = *(char *)((int)&uStack_510 + iVar9 + 0x515);
                if ((byte)(cVar10 + 0x9fU) < 0x1a) {
                  cVar10 = cVar10 + -0x20;
                }
                if (cVar10 == 'A') {
                  cVar10 = *(char *)((int)&uStack_510 + iVar9 + 0x516);
                  if ((byte)(cVar10 + 0x9fU) < 0x1a) {
                    cVar10 = cVar10 + -0x20;
                  }
                  if (cVar10 == 'U') {
                    cVar10 = *(char *)((int)&uStack_510 + iVar9 + 0x517);
                    if ((byte)(cVar10 + 0x9fU) < 0x1a) {
                      cVar10 = cVar10 + -0x20;
                    }
                    if (cVar10 == 'X') {
                      bVar30 = true;
                    }
                  }
                }
              }
              uVar25 = uVar25 + 1;
              if (uVar25 < uVar26) {
                bVar11 = *(byte *)((int)&uStack_510 + iVar9 + 0x515);
                if ((byte)(bVar11 + 0x9f) < 0x1a) {
                  bVar11 = bVar11 - 0x20;
                }
                if (bVar11 < 0x4c) {
                  if (bVar11 == 0x43) {
                    cVar10 = *(char *)((int)&uStack_510 + iVar9 + 0x516);
                    if ((byte)(cVar10 + 0x9fU) < 0x1a) {
                      cVar10 = cVar10 + -0x20;
                    }
                    if (cVar10 == 'D') {
                      bVar6 = true;
                    }
                  }
                  else if (bVar11 == 0x46) {
                    cVar10 = *(char *)((int)&uStack_510 + iVar9 + 0x516);
                    if ((byte)(cVar10 + 0x9fU) < 0x1a) {
                      cVar10 = cVar10 + -0x20;
                    }
                    if (cVar10 == 'M') {
                      bVar4 = true;
                    }
                  }
                }
                else if (bVar11 == 0x4c) {
                  cVar10 = *(char *)((int)&uStack_510 + iVar9 + 0x516);
                  if ((byte)(cVar10 + 0x9fU) < 0x1a) {
                    cVar10 = cVar10 + -0x20;
                  }
                  if (cVar10 == 'W') {
                    bVar7 = true;
                  }
                }
                else if (bVar11 == 0x4d) {
                  cVar10 = *(char *)((int)&uStack_510 + iVar9 + 0x516);
                  if ((byte)(cVar10 + 0x9fU) < 0x1a) {
                    cVar10 = cVar10 + -0x20;
                  }
                  if (cVar10 == 'W') {
                    bVar5 = true;
                  }
                }
              }
            } while (uVar26 != uVar25);
            uVar25 = 5;
            if ((((bVar30) || (uVar25 = 1, bVar4)) || (uVar25 = 2, bVar5)) ||
               ((uVar25 = 3, bVar7 || (uVar25 = 4, bVar6)))) {
              g_source = uVar25;
              log_num("SOURCE_DYNAMIC_MAP",uVar18,uVar15,1,uVar25);
              publish();
              pcVar13 = "PUBLISH_SOURCE";
              uVar18 = g_temp;
              uVar15 = g_source;
              uVar25 = g_ice;
              uVar26 = g_template;
            }
            else {
              pcVar13 = "SOURCE_DYNAMIC_IGNORE";
              uVar25 = 1;
            }
            log_num(pcVar13,uVar18,uVar15,uVar25,uVar26);
          }
          else if ((uVar15 == 0x2c && cVar10 == '\x02') && (uVar26 != 0)) {
            uVar15 = (uint)*pbVar16;
            if (1 < uVar26) {
              uVar15 = (uint)CONCAT11(*(undefined1 *)((int)&uStack_510 + uVar8 + 2),*pbVar16);
            }
            uVar26 = 4;
            if (1 < uVar15 - 200) {
              if (uVar15 != 0xcb) break;
              uVar26 = 5;
            }
            pcVar13 = "SOURCE_PROP";
            uVar19 = 0x2c;
            uVar21 = 2;
            g_source = uVar26;
            goto LAB_00021074;
          }
          break;
        case 0xe:
          if ((int)uVar24 < 6) {
            pcVar13 = "TRUNC_SETIDX_HDR";
            goto LAB_0002173c;
          }
          uVar18 = (uint)*(byte *)((int)&uStack_510 + uVar8 + 1);
          uVar29 = uVar18 + 6;
          if (uVar29 <= uVar24) {
            uVar15 = (uint)*(byte *)((int)&uStack_514 + uVar8 + 1);
            uVar20 = *(undefined1 *)((int)&uStack_514 + uVar8 + 3);
            cVar10 = *(char *)((int)&uStack_514 + uVar8 + 2);
            cVar3 = *(char *)((int)&uStack_510 + uVar8);
            g_viewer = uVar15;
            log_num(&DAT_00010248,uVar8,0xe,uVar29,bVar11 >> 7);
            log_num("SETIDX",uVar15,cVar10,uVar20,cVar3);
            if ((uVar18 == 0) || (cVar10 != ',' || cVar3 != '\x02')) break;
            bVar11 = *(byte *)((int)&uStack_510 + uVar8 + 2);
            uVar26 = (uint)bVar11;
            if (1 < uVar18) {
              uVar26 = (uint)CONCAT11(*(undefined1 *)((int)&uStack_510 + uVar8 + 3),bVar11);
            }
            uVar18 = 4;
            if (uVar26 - 200 < 2) {
LAB_00020e9c:
              g_source = uVar18;
              publish();
            }
            else if (uVar26 == 0xcb) {
              uVar18 = 5;
              goto LAB_00020e9c;
            }
            log_num("SOURCE_GFX",uVar15,uVar20,2,uVar26);
            break;
          }
          pcVar13 = "TRUNC_SETIDX_DATA";
          uVar28 = uVar8;
          uVar15 = uVar24;
          goto LAB_00020804;
        case 0x11:
          if ((int)uVar24 < 2) {
            pcVar13 = "TRUNC_UPDATE";
            goto LAB_0002173c;
          }
          g_viewer = (uint)*(byte *)((int)&uStack_514 + uVar8 + 1);
          uVar29 = 2;
          log_num(&DAT_00010248,uVar8,0x11,2,bVar11 >> 7);
          pcVar13 = "UPDATE_SCREEN";
          uVar19 = 0;
          uVar21 = 0;
          uVar15 = 0;
          uVar18 = g_viewer;
LAB_00021074:
          log_num(pcVar13,uVar18,uVar19,uVar21,uVar15);
          publish();
          break;
        case 0x12:
          if ((int)uVar24 < 3) {
            pcVar13 = "TRUNC_RESET";
            goto LAB_0002173c;
          }
          log_num(&DAT_00010248,uVar8,0x12,3,bVar11 >> 7);
          uVar18 = (uint)*(byte *)((int)&uStack_514 + uVar8 + 2);
          pcVar13 = "RESET_PROPERTIES";
          uVar29 = 3;
          uVar15 = (uint)*(byte *)((int)&uStack_514 + uVar8 + 1);
LAB_00020dd0:
          uVar20 = 0;
LAB_00020f04:
          log_num(pcVar13,uVar15,uVar18,uVar20,0);
          break;
        case 0x13:
          if ((int)uVar24 < 4) {
            pcVar13 = "TRUNC_RESETIDX";
            goto LAB_0002173c;
          }
          log_num(&DAT_00010248,uVar8,0x13,4,bVar11 >> 7);
          uVar15 = (uint)*(byte *)((int)&uStack_514 + uVar8 + 1);
          uVar18 = (uint)*(byte *)((int)&uStack_514 + uVar8 + 2);
          uVar20 = *(undefined1 *)((int)&uStack_514 + uVar8 + 3);
          pcVar13 = "RESET_INDEXED";
          uVar29 = 4;
          goto LAB_00020f04;
        }
LAB_0002168c:
        if ((int)uVar24 < (int)uVar29) {
          pcVar13 = "BAD_METHOD_SIZE";
          uVar18 = uVar24;
          uVar15 = uVar8;
LAB_00021750:
          log_num(pcVar13,uVar28,uVar29,uVar18,uVar15);
          break;
        }
        uVar8 = uVar29 + uVar8;
      } while ((int)uVar8 < 10);
LAB_00021754:
      publish();
    }
    software_interrupt(0x900066);
  } while( true );
}



/* ===== 0x21d84 | dec_append | ev_8e0cd105a55e797336133e1a6d053031954a5c3119c4a19ffd55e13a2a992e4c ===== */

void dec_append(int param_1,int param_2,int param_3,uint param_4)

{
  bool bVar1;
  bool bVar2;
  int iVar3;
  int iVar4;
  
  if ((0x7fffffff < param_4) && (param_4 = -param_4, param_2 < param_3 + -1)) {
    *(undefined1 *)(param_1 + param_2) = 0x2d;
    param_2 = param_2 + 1;
  }
  iVar3 = 0;
  bVar1 = false;
  do {
    iVar4 = 0;
    for (; (uint)(&DAT_000104ec)[iVar3] <= param_4; param_4 = param_4 - (&DAT_000104ec)[iVar3]) {
      iVar4 = iVar4 + 1;
    }
    if (((iVar3 == 9) || (bVar2 = iVar4 != 0 || bVar1, bVar1 = false, bVar2)) &&
       (bVar1 = true, param_2 < param_3 + -1)) {
      *(char *)(param_1 + param_2) = (char)iVar4 + '0';
      param_2 = param_2 + 1;
    }
    iVar3 = iVar3 + 1;
  } while (iVar3 != 10);
  return;
}



/* ===== 0x21e10 | log_tx | ev_91bd0f3e64c60424f4be4d793aebc2a2bb905c539d6aea9cde394350e244c581 ===== */

void log_tx(char *param_1,undefined1 *param_2,int param_3)

{
  uint uVar1;
  int iVar2;
  uint uVar3;
  int iVar4;
  uint uVar5;
  char *pcVar6;
  char cVar7;
  char local_fc [220];
  
  cVar7 = *param_1;
  if (cVar7 == '\0') {
    uVar3 = 0;
  }
  else {
    uVar3 = 0;
    do {
      uVar1 = uVar3;
      local_fc[uVar1] = cVar7;
      if (0xd9 < uVar1) break;
      cVar7 = param_1[uVar1 + 1];
      uVar3 = uVar1 + 1;
    } while (cVar7 != '\0');
    uVar5 = 0xdb;
    uVar3 = uVar1 + 1;
    if (0xd9 < uVar1) goto LAB_00021ea8;
  }
  uVar5 = uVar3;
  uVar3 = 0xda - uVar5;
  cVar7 = ' ';
  if (2 < uVar3) {
    uVar3 = 3;
  }
  iVar2 = uVar3 + 1;
  pcVar6 = " rc=";
  do {
    pcVar6 = pcVar6 + 1;
    local_fc[uVar5] = cVar7;
    cVar7 = *pcVar6;
    iVar2 = iVar2 + -1;
    uVar5 = uVar5 + 1;
  } while (iVar2 != 0);
LAB_00021ea8:
  iVar2 = dec_append(local_fc,uVar5,0xdc);
  if (iVar2 < 0xdb) {
    pcVar6 = " n=";
    uVar3 = 0xda - iVar2;
    cVar7 = ' ';
    if (1 < uVar3) {
      uVar3 = 2;
    }
    iVar4 = uVar3 + 1;
    do {
      pcVar6 = pcVar6 + 1;
      local_fc[iVar2] = cVar7;
      cVar7 = *pcVar6;
      iVar4 = iVar4 + -1;
      iVar2 = iVar2 + 1;
    } while (iVar4 != 0);
  }
  iVar2 = dec_append(local_fc,iVar2,0xdc,param_3);
  if (iVar2 < 0xdb) {
    local_fc[iVar2] = ':';
    iVar2 = iVar2 + 1;
  }
  if (0 < param_3) {
    do {
      iVar2 = hexbyte_append(local_fc,iVar2,0xdc,*param_2);
      param_3 = param_3 + -1;
      param_2 = param_2 + 1;
    } while (param_3 != 0);
  }
  local_fc[iVar2] = '\0';
  log_line(local_fc);
  return;
}



/* ===== 0x21f64 | log_text | ev_f733e52f0477bb9a9ee1cab82974baf9a1d3898dce6fc3c9c6f52edad3b534c6 ===== */

void log_text(byte *param_1,int param_2,int param_3)

{
  uint uVar1;
  uint uVar2;
  int iVar3;
  int iVar4;
  byte bVar5;
  int iVar6;
  uint uVar7;
  byte local_cc [180];
  
  bVar5 = *param_1;
  uVar1 = 0;
  uVar7 = 0;
  if (bVar5 != 0) {
    uVar7 = 0;
    do {
      uVar2 = uVar7;
      uVar7 = uVar2 + 1;
      local_cc[uVar2] = bVar5;
      if (0xb1 < uVar2) break;
      bVar5 = param_1[uVar2 + 1];
    } while (bVar5 != 0);
    iVar3 = 0xb3;
    if (0xb1 < uVar2) goto LAB_00022070;
  }
  uVar2 = 0xb2 - uVar7;
  bVar5 = 0x20;
  if (5 < uVar2) {
    uVar2 = 6;
  }
  do {
    local_cc[((uVar7 - 0xcc) - uVar1) + 0xcc] = bVar5;
    iVar3 = -uVar1;
    uVar1 = uVar1 - 1;
    bVar5 = (&DAT_000101d4)[iVar3];
  } while (~uVar2 != uVar1);
  iVar3 = uVar7 - uVar1;
  iVar6 = param_3;
  if (0x4f < param_3) {
    iVar6 = 0x50;
  }
  if ((param_3 < 1) || (0xb0 < iVar3 + -1)) {
    if (0xb1 < iVar3 + -1) goto LAB_00022070;
  }
  else {
    iVar4 = 0;
    do {
      bVar5 = *(byte *)(param_2 + iVar4);
      if (0x5e < bVar5 - 0x20) {
        bVar5 = 0x2e;
      }
      local_cc[((iVar4 + (uVar7 - 0xcc)) - uVar1) + 0xcc] = bVar5;
      iVar3 = iVar4 + 1;
    } while ((iVar3 < iVar6) && (uVar2 = (uVar7 - uVar1) + iVar4, iVar4 = iVar3, uVar2 < 0xb1));
    iVar3 = (uVar7 - uVar1) + iVar3;
  }
  local_cc[iVar3] = 0x22;
  iVar3 = iVar3 + 1;
LAB_00022070:
  local_cc[iVar3] = 0;
  log_line();
  return;
}



/* ===== 0x2208c | hexbyte_append | ev_078f574b6bbdd256645d34516f6f0fc6d0c9f51f686cc1fea26d8350bafcd13c ===== */

int hexbyte_append(int param_1,int param_2,int param_3,uint param_4)

{
  param_3 = param_3 + -1;
  if (param_2 < param_3) {
    *(undefined1 *)(param_1 + param_2) = 0x20;
    param_2 = param_2 + 1;
  }
  if (param_2 < param_3) {
    *(char *)(param_1 + param_2) = "0123456789abcdef"[param_4 >> 4];
    param_2 = param_2 + 1;
  }
  if (param_2 < param_3) {
    *(char *)(param_1 + param_2) = "0123456789abcdef"[param_4 & 0xf];
    param_2 = param_2 + 1;
  }
  return param_2;
}



