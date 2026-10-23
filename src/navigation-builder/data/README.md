# Bundled Global data

`countries.geojson` is Natural Earth's generalized 1:10m Admin-0 country data,
public domain. Source:
https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_10m_admin_0_countries.geojson

SHA-256: `239eec57ac17f100a11e2536cffc56752c318b50ae765b0918ff7aab4ce8f255`.
Terms: https://www.naturalearthdata.com/about/.

`global-fonts.cpf` contains Noto glyph coverage and OpenType layout tables baked
at 11/13/15 pixels. Its JSON manifest records the exact font URLs/hashes and pack
hash. Individual copyright/licence notices are in `font-notices.json`; keep them
and `OFL.txt` with any redistributed font pack. Normal navigation builds verify
and copy this pack; users need not find or install fonts separately.

`camera-policy.json` records the exact-location exclusion policy and review date.
It is product configuration, not a worldwide legal permission list. Runtime
policy is carried in the generated `regions.cpg`. See the audit for limitations.

Reviewed mph fallback references beyond the usual GB/US profiles include:

- Liberia vehicle law: https://www.rss.gov.lr/sites/default/files/documents/Vehicle-and-Traffic-Law.pdf
- Bahamas traffic regulations: https://www.roadtraffic.gov.bs/wp-content/uploads/2016/04/MotorVehicleandSpeedLimitRegulations_1.pdf
- Cayman Islands Traffic Act: https://gov.ky/documents/35692/0/Traffic%2BAct%2B%282026%2BRevision%29%2C.pdf/5efb6dce-40fa-913f-015f-7931e676c457
- Federated States of Micronesia, Yap code: https://www.fsmlaw.org/yap/code/title25/T25_Ch04.htm

Native mph tags also inform Auto on other matched roads. These profiles are
road-unit defaults, not a complete database of limits or national traffic rules.
