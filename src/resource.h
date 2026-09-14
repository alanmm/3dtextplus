#ifndef M3DT_RESOURCE_H
#define M3DT_RESOURCE_H

#define IDD_CONFIG        101
#define IDD_TAB_CONTENT   110
#define IDD_TAB_MOTION    111
#define IDD_TAB_MATERIAL  112
#define IDD_TAB_GEOMETRY  113
#define IDD_TAB_EFFECTS   114
#define IDD_TAB_PERF      115
#define IDD_TAB_POST      116
#define IDD_TAB_BG        117
#define IDD_TAB_PARTICLES 118

#define IDC_TABS          1000
#define IDC_PREVIEW       1001
#define IDC_APPLY         1002

/* aba Conteudo */
#define IDC_TEXT          1100
#define IDC_FONT          1101
#define IDC_BOLD          1102
#define IDC_ITALIC        1103
#define IDC_COLOR         1104
#define IDC_CONTMODE      1105
#define IDC_CLOCKDATE     1106
#define IDC_CLOCKSEC      1107
#define IDC_TEXTLABEL     1108
#define IDC_FONTLABEL     1109
#define IDC_SVGPATHLABEL  1110
#define IDC_SVGPATH       1111
#define IDC_SVGPICK       1112
#define IDC_SVGCLEAR      1113
#define IDC_SVGCOLORLABEL 1114
#define IDC_SVGCOLORMODE  1115
#define IDC_MESHPATHLABEL  1116
#define IDC_MESHPATH       1117
#define IDC_MESHPICK       1118
#define IDC_MESHCLEAR      1119
#define IDC_MESHSCALELABEL 1120
#define IDC_MESHSCALE_VAL  1121
#define IDC_MESHSCALE      1122
#define IDC_MESHUSEMAT     1123
#define IDC_MODE_LABEL     1124
#define IDC_COLOR_LABEL    1125

/* aba Movimento */
#define IDC_DEPTH         1200
#define IDC_DEPTH_VAL     1201
#define IDC_ANGLE         1202
#define IDC_ANGLE_VAL     1203
#define IDC_TILT          1204
#define IDC_TILT_VAL      1205
#define IDC_PERIOD        1206
#define IDC_PERIOD_VAL    1207
#define IDC_ANGLE_LABEL   1208
#define IDC_TILT_LABEL    1209
#define IDC_PERIOD_LABEL  1210

/* aba Material */
#define IDC_MATMODE       1300
#define IDC_METAL         1301
#define IDC_METAL_VAL     1302
#define IDC_ROUGH         1303
#define IDC_ROUGH_VAL     1304
#define IDC_ENVPATH       1305
#define IDC_ENVPICK       1306
#define IDC_ENVCLEAR      1307
#define IDC_MATERIAL_LABEL 1308
#define IDC_METAL_LABEL    1309
#define IDC_ROUGH_LABEL    1310
#define IDC_ENV_LABEL      1311
#define IDC_ENVMODE_EMBED  1312
#define IDC_ENVMODE_CUSTOM 1313
#define IDC_ENVMODE_NONE   1314

/* aba Geometria */
#define IDC_BEVELMODE     1400
#define IDC_BSIZE         1401
#define IDC_BSIZE_VAL     1402
#define IDC_BDEPTH        1403
#define IDC_BDEPTH_VAL    1404
#define IDC_BSEG          1405
#define IDC_BSEG_VAL      1406
#define IDC_SHELL         1407
#define IDC_WALL          1408
#define IDC_WALL_VAL      1409
#define IDC_QUALITY       1410
#define IDC_DEPTH_LABEL   1411
#define IDC_BEVEL_LABEL   1412
#define IDC_BSIZE_LABEL   1413
#define IDC_BDEPTH_LABEL  1414
#define IDC_BSEG_LABEL    1415
#define IDC_WALL_LABEL    1416
#define IDC_QUALITY_LABEL 1417

/* aba Efeitos */
#define IDC_BLOOM         1500
#define IDC_BTHRESH       1501
#define IDC_BTHRESH_VAL   1502
#define IDC_BINT          1503
#define IDC_BINT_VAL      1504
#define IDC_BRAD          1505
#define IDC_BRAD_VAL      1506
#define IDC_STREAKMODE    1510
#define IDC_SINT          1511
#define IDC_SINT_VAL      1512
#define IDC_SLEN          1513
#define IDC_SLEN_VAL      1514
#define IDC_BTHRESH_LABEL 1520
#define IDC_BINT_LABEL    1521
#define IDC_BRAD_LABEL    1522
#define IDC_STREAKS_LABEL 1523
#define IDC_SINT_LABEL    1524
#define IDC_SLEN_LABEL    1525
#define IDC_EFFECTS_HINT  1526

/* aba Desempenho */
#define IDC_FPSCAP        1600
#define IDC_VSYNC         1601
#define IDC_MSAA          1602
#define IDC_RSCALE        1603
#define IDC_RSCALE_VAL    1604
#define IDC_AUTOQ         1605
#define IDC_FPS_LABEL     1606
#define IDC_MSAA_LABEL    1607
#define IDC_RSCALE_LABEL  1608
#define IDC_PERF_HINT     1609
#define IDC_LANGUAGE_LABEL 1610
#define IDC_LANGUAGE       1611

/* aba Pos */
#define IDC_CHROMA        1700
#define IDC_CSTR          1701
#define IDC_CSTR_VAL      1702
#define IDC_VIGNETTE      1703
#define IDC_VAMT          1704
#define IDC_VAMT_VAL      1705
#define IDC_FXAA          1706
#define IDC_CSTR_LABEL    1710
#define IDC_VAMT_LABEL    1711
#define IDC_POST_HINT     1712

/* aba Fundo */
#define IDC_BGTYPE        1800
#define IDC_BGCOLOR1      1801
#define IDC_BGCOLOR2      1802
#define IDC_BGANGLE       1803
#define IDC_BGANGLE_VAL   1804
#define IDC_BGIMGPATH     1805
#define IDC_BGIMGPICK     1806
#define IDC_BGIMGCLEAR    1807
#define IDC_BGFIT         1808
#define IDC_BGPAN         1809
#define IDC_BGPAN_VAL     1810
#define IDC_BGNEBCOLOR1   1811
#define IDC_BGNEBCOLOR2   1812
#define IDC_BGTYPE_LABEL   1820
#define IDC_BGCOLOR1_LABEL 1821
#define IDC_BGCOLOR2_LABEL 1822
#define IDC_BGANGLE_LABEL  1823
#define IDC_BGIMAGE_LABEL  1824
#define IDC_BGFIT_LABEL    1825
#define IDC_BGPAN_LABEL    1826
#define IDC_BGNEBULA_LABEL 1827

/* aba Particulas */
#define IDC_PARTON        1900
#define IDC_PARTKIND      1901
#define IDC_PARTDENS      1902
#define IDC_PARTDENS_VAL  1903
#define IDC_PARTSPEED     1904
#define IDC_PARTSPEED_VAL 1905
#define IDC_PARTSIZE      1906
#define IDC_PARTSIZE_VAL  1907
#define IDC_PARTOPACITY     1908
#define IDC_PARTOPACITY_VAL 1909
#define IDC_PARTKIND_LABEL     1920
#define IDC_PARTDENS_LABEL     1921
#define IDC_PARTSPEED_LABEL    1922
#define IDC_PARTSIZE_LABEL     1923
#define IDC_PARTOPACITY_LABEL  1924

#define IDD_PRESET_NAME    119

#define IDC_PRESET_LABEL      2000
#define IDC_PRESET_COMBO      2001
#define IDC_PRESET_SAVE       2002
#define IDC_PRESET_DELETE     2003
#define IDC_PRESET_IMPORT     2004
#define IDC_PRESET_EXPORT     2005
#define IDC_PRESET_NAME_LABEL 2006
#define IDC_PRESET_NAME_EDIT  2007

#endif
