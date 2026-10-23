# Speed camera support

Reviewed 20 October 2026. This table controls GPS database alerts, including camera CSV imports. It does not describe radar receivers or jammers. The interface language never changes the location policy.

**Enabled (19 countries):** Austria (AT), Belgium (BE), Czechia (CZ), Kingdom of Denmark (DK), Spain (ES), Finland (FI), United Kingdom (GB), Croatia (HR), Hungary (HU), Italy (IT), Luxembourg (LU), Kingdom of the Netherlands (NL), Norway (NO), Poland (PL), Portugal (PT), Serbia (RS), Russia (RU), Sweden (SE), Slovenia (SI).

**Exact alerts blocked:** Switzerland (CH), Republic of Cyprus (CY), Germany (DE), France (FR). No invented French danger-zone mode is substituted.

**Other territories:** disabled pending a GPS-specific policy review. This is a product decision, not a statement that alerts are illegal there. The JSON contains an explicit state for all 260 country profiles. Adding a language does not establish legal permission in countries using it.

## Supported languages and examples

The examples make the decisions visible across the interface languages. They are not an exhaustive list of places where those languages are spoken.

| Language | Enabled examples | Blocked examples | Pending examples |
|---|---|---|---|
| English | United Kingdom (GB) | — | Australia (AU), Canada (CA), Republic of Ireland (IE), New Zealand (NZ), United States of America (US), South Africa (ZA) |
| Français | Belgium (BE), Luxembourg (LU) | Switzerland (CH), France (FR) | Canada (CA), Monaco (MC) |
| Español | Spain (ES) | — | Argentina (AR), Chile (CL), Colombia (CO), Mexico (MX), Peru (PE), Uruguay (UY) |
| Deutsch | Austria (AT), Belgium (BE), Luxembourg (LU) | Switzerland (CH), Germany (DE) | Liechtenstein (LI) |
| Italiano | Italy (IT) | Switzerland (CH) | San Marino (SM), Vatican City (VA) |
| Português | Portugal (PT) | — | Angola (AO), Brazil (BR), Cape Verde (CV), Guinea-Bissau (GW), Mozambique (MZ), São Tomé and Principe (ST), East Timor (TL) |
| Nederlands | Belgium (BE), Kingdom of the Netherlands (NL) | — | Aruba (AW), Curaçao (CW), Suriname (SR), Sint Maarten (SX) |
| Polski | Poland (PL) | — | — |
| Türkçe | — | Republic of Cyprus (CY) | Turkey (TR) |
| Ελληνικά | — | Republic of Cyprus (CY) | Greece (GR) |
| Українська | — | — | Ukraine (UA) |
| Русский | Russia (RU) | — | Belarus (BY), Kyrgyzstan (KG), Kazakhstan (KZ) |
| العربية | — | — | United Arab Emirates (AE), Bahrain (BH), Djibouti (DJ), Algeria (DZ), Egypt (EG), Iraq (IQ), Jordan (JO), Comoros (KM), Kuwait (KW), Lebanon (LB), Libya (LY), Morocco (MA), Mauritania (MR), Oman (OM), Palestine (PS), Qatar (QA), Saudi Arabia (SA), Sudan (SD), Somalia (SO), Syria (SY), Tunisia (TN), Yemen (YE) |
| 简体中文 | — | — | People's Republic of China (CN), Hong Kong (HK), Macau (MO), Singapore (SG), Taiwan (TW) |
| 日本語 | — | — | Japan (JP) |
| 한국어 | — | — | South Korea (KR) |
| हिन्दी | — | — | India (IN) |
| עברית | — | — | Israel (IL) |

## Enforcement and updates

The one-PBF builder writes country policy flags into `regions.cpg` and filters exact locations from `cameras.cpc`. Both ends of an average-speed section must pass. A conservative 0.1° exclusion buffer surrounds explicitly restricted countries; nearby permitted locations can therefore be omitted too. The native UI suppresses alerts when the resolved country flag is disabled, regardless of language or the user's Visual/Phone audio preference. Unresolved/offshore positions fail closed. Generalized coastlines and borders can leave gaps; they are not a legal boundary survey.

Future builds use this list automatically. Existing navigation bundles retain their stored policy until their camera/country metadata is refreshed; a UI language change alone cannot update that metadata. Install a complete validated navigation bundle through the installer. This change does not require rebuilding road geometry or route graphs. OSM coverage remains incomplete and is not live enforcement data.

## Review evidence

The allowlist uses the GPS/POI distinctions in [ÖAMTC's legal guidance](https://www.oeamtc.at/thema/vorschriften-strafen/radarwarner-illegale-verwendung-kann-bis-zu-10-000-euro-kosten-16184654), supplemented by [ADAC's explicit Czech POI guidance](https://www.adac.de/verkehr/recht/verkehrsvorschriften-deutschland/radarwarner/), [Yanosik's Poland statement](https://yanosik.pl/wsparcie/alert), and the [Italian Interior Ministry circular](https://rivistagiuridica.aci.it/fileadmin/Documenti/Circ_autovelox_navigatori_02.pdf). The existing GB and Spain reviews remain recorded in the JSON. Cyprus is excluded following [TomTom's legal guidance](https://help.tomtom.com/hc/en-gb/articles/360013899980-Are-speed-camera-warnings-legal); France, Germany and Switzerland retain their original legislative/customs sources. Guidance can change; the checked date and source links are versioned in `navigation-builder/data/camera-policy.json`. Manufacturer availability by itself is not treated as legal clearance.

## Checks

`tools/verify_camera_policy.py` checks real OSM and CSV exports, average-section endpoint filtering, the restricted-border buffer, and 720 native country/language combinations across 40 representative interior locations. The native renderer/alert code uses the actual exported CPG/CPC files. These are host tests, not physical vehicle validation.
