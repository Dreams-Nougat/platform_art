/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

public class Main {

  public static void assertEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void main(String[] args) {
    // Sum[i = 0..999](i) = 999 * 1000 / 2 = 499500L.
    assertEquals(499500L, HugeMethod());
  }

  // This method cannot be optimized because of its huge size.
  static long HugeMethod() {
    long l0 = 0;
    long l1 = 1;
    long l2 = 2;
    long l3 = 3;
    long l4 = 4;
    long l5 = 5;
    long l6 = 6;
    long l7 = 7;
    long l8 = 8;
    long l9 = 9;
    long l10 = 10;
    long l11 = 11;
    long l12 = 12;
    long l13 = 13;
    long l14 = 14;
    long l15 = 15;
    long l16 = 16;
    long l17 = 17;
    long l18 = 18;
    long l19 = 19;
    long l20 = 20;
    long l21 = 21;
    long l22 = 22;
    long l23 = 23;
    long l24 = 24;
    long l25 = 25;
    long l26 = 26;
    long l27 = 27;
    long l28 = 28;
    long l29 = 29;
    long l30 = 30;
    long l31 = 31;
    long l32 = 32;
    long l33 = 33;
    long l34 = 34;
    long l35 = 35;
    long l36 = 36;
    long l37 = 37;
    long l38 = 38;
    long l39 = 39;
    long l40 = 40;
    long l41 = 41;
    long l42 = 42;
    long l43 = 43;
    long l44 = 44;
    long l45 = 45;
    long l46 = 46;
    long l47 = 47;
    long l48 = 48;
    long l49 = 49;
    long l50 = 50;
    long l51 = 51;
    long l52 = 52;
    long l53 = 53;
    long l54 = 54;
    long l55 = 55;
    long l56 = 56;
    long l57 = 57;
    long l58 = 58;
    long l59 = 59;
    long l60 = 60;
    long l61 = 61;
    long l62 = 62;
    long l63 = 63;
    long l64 = 64;
    long l65 = 65;
    long l66 = 66;
    long l67 = 67;
    long l68 = 68;
    long l69 = 69;
    long l70 = 70;
    long l71 = 71;
    long l72 = 72;
    long l73 = 73;
    long l74 = 74;
    long l75 = 75;
    long l76 = 76;
    long l77 = 77;
    long l78 = 78;
    long l79 = 79;
    long l80 = 80;
    long l81 = 81;
    long l82 = 82;
    long l83 = 83;
    long l84 = 84;
    long l85 = 85;
    long l86 = 86;
    long l87 = 87;
    long l88 = 88;
    long l89 = 89;
    long l90 = 90;
    long l91 = 91;
    long l92 = 92;
    long l93 = 93;
    long l94 = 94;
    long l95 = 95;
    long l96 = 96;
    long l97 = 97;
    long l98 = 98;
    long l99 = 99;
    long l100 = 100;
    long l101 = 101;
    long l102 = 102;
    long l103 = 103;
    long l104 = 104;
    long l105 = 105;
    long l106 = 106;
    long l107 = 107;
    long l108 = 108;
    long l109 = 109;
    long l110 = 110;
    long l111 = 111;
    long l112 = 112;
    long l113 = 113;
    long l114 = 114;
    long l115 = 115;
    long l116 = 116;
    long l117 = 117;
    long l118 = 118;
    long l119 = 119;
    long l120 = 120;
    long l121 = 121;
    long l122 = 122;
    long l123 = 123;
    long l124 = 124;
    long l125 = 125;
    long l126 = 126;
    long l127 = 127;
    long l128 = 128;
    long l129 = 129;
    long l130 = 130;
    long l131 = 131;
    long l132 = 132;
    long l133 = 133;
    long l134 = 134;
    long l135 = 135;
    long l136 = 136;
    long l137 = 137;
    long l138 = 138;
    long l139 = 139;
    long l140 = 140;
    long l141 = 141;
    long l142 = 142;
    long l143 = 143;
    long l144 = 144;
    long l145 = 145;
    long l146 = 146;
    long l147 = 147;
    long l148 = 148;
    long l149 = 149;
    long l150 = 150;
    long l151 = 151;
    long l152 = 152;
    long l153 = 153;
    long l154 = 154;
    long l155 = 155;
    long l156 = 156;
    long l157 = 157;
    long l158 = 158;
    long l159 = 159;
    long l160 = 160;
    long l161 = 161;
    long l162 = 162;
    long l163 = 163;
    long l164 = 164;
    long l165 = 165;
    long l166 = 166;
    long l167 = 167;
    long l168 = 168;
    long l169 = 169;
    long l170 = 170;
    long l171 = 171;
    long l172 = 172;
    long l173 = 173;
    long l174 = 174;
    long l175 = 175;
    long l176 = 176;
    long l177 = 177;
    long l178 = 178;
    long l179 = 179;
    long l180 = 180;
    long l181 = 181;
    long l182 = 182;
    long l183 = 183;
    long l184 = 184;
    long l185 = 185;
    long l186 = 186;
    long l187 = 187;
    long l188 = 188;
    long l189 = 189;
    long l190 = 190;
    long l191 = 191;
    long l192 = 192;
    long l193 = 193;
    long l194 = 194;
    long l195 = 195;
    long l196 = 196;
    long l197 = 197;
    long l198 = 198;
    long l199 = 199;
    long l200 = 200;
    long l201 = 201;
    long l202 = 202;
    long l203 = 203;
    long l204 = 204;
    long l205 = 205;
    long l206 = 206;
    long l207 = 207;
    long l208 = 208;
    long l209 = 209;
    long l210 = 210;
    long l211 = 211;
    long l212 = 212;
    long l213 = 213;
    long l214 = 214;
    long l215 = 215;
    long l216 = 216;
    long l217 = 217;
    long l218 = 218;
    long l219 = 219;
    long l220 = 220;
    long l221 = 221;
    long l222 = 222;
    long l223 = 223;
    long l224 = 224;
    long l225 = 225;
    long l226 = 226;
    long l227 = 227;
    long l228 = 228;
    long l229 = 229;
    long l230 = 230;
    long l231 = 231;
    long l232 = 232;
    long l233 = 233;
    long l234 = 234;
    long l235 = 235;
    long l236 = 236;
    long l237 = 237;
    long l238 = 238;
    long l239 = 239;
    long l240 = 240;
    long l241 = 241;
    long l242 = 242;
    long l243 = 243;
    long l244 = 244;
    long l245 = 245;
    long l246 = 246;
    long l247 = 247;
    long l248 = 248;
    long l249 = 249;
    long l250 = 250;
    long l251 = 251;
    long l252 = 252;
    long l253 = 253;
    long l254 = 254;
    long l255 = 255;
    long l256 = 256;
    long l257 = 257;
    long l258 = 258;
    long l259 = 259;
    long l260 = 260;
    long l261 = 261;
    long l262 = 262;
    long l263 = 263;
    long l264 = 264;
    long l265 = 265;
    long l266 = 266;
    long l267 = 267;
    long l268 = 268;
    long l269 = 269;
    long l270 = 270;
    long l271 = 271;
    long l272 = 272;
    long l273 = 273;
    long l274 = 274;
    long l275 = 275;
    long l276 = 276;
    long l277 = 277;
    long l278 = 278;
    long l279 = 279;
    long l280 = 280;
    long l281 = 281;
    long l282 = 282;
    long l283 = 283;
    long l284 = 284;
    long l285 = 285;
    long l286 = 286;
    long l287 = 287;
    long l288 = 288;
    long l289 = 289;
    long l290 = 290;
    long l291 = 291;
    long l292 = 292;
    long l293 = 293;
    long l294 = 294;
    long l295 = 295;
    long l296 = 296;
    long l297 = 297;
    long l298 = 298;
    long l299 = 299;
    long l300 = 300;
    long l301 = 301;
    long l302 = 302;
    long l303 = 303;
    long l304 = 304;
    long l305 = 305;
    long l306 = 306;
    long l307 = 307;
    long l308 = 308;
    long l309 = 309;
    long l310 = 310;
    long l311 = 311;
    long l312 = 312;
    long l313 = 313;
    long l314 = 314;
    long l315 = 315;
    long l316 = 316;
    long l317 = 317;
    long l318 = 318;
    long l319 = 319;
    long l320 = 320;
    long l321 = 321;
    long l322 = 322;
    long l323 = 323;
    long l324 = 324;
    long l325 = 325;
    long l326 = 326;
    long l327 = 327;
    long l328 = 328;
    long l329 = 329;
    long l330 = 330;
    long l331 = 331;
    long l332 = 332;
    long l333 = 333;
    long l334 = 334;
    long l335 = 335;
    long l336 = 336;
    long l337 = 337;
    long l338 = 338;
    long l339 = 339;
    long l340 = 340;
    long l341 = 341;
    long l342 = 342;
    long l343 = 343;
    long l344 = 344;
    long l345 = 345;
    long l346 = 346;
    long l347 = 347;
    long l348 = 348;
    long l349 = 349;
    long l350 = 350;
    long l351 = 351;
    long l352 = 352;
    long l353 = 353;
    long l354 = 354;
    long l355 = 355;
    long l356 = 356;
    long l357 = 357;
    long l358 = 358;
    long l359 = 359;
    long l360 = 360;
    long l361 = 361;
    long l362 = 362;
    long l363 = 363;
    long l364 = 364;
    long l365 = 365;
    long l366 = 366;
    long l367 = 367;
    long l368 = 368;
    long l369 = 369;
    long l370 = 370;
    long l371 = 371;
    long l372 = 372;
    long l373 = 373;
    long l374 = 374;
    long l375 = 375;
    long l376 = 376;
    long l377 = 377;
    long l378 = 378;
    long l379 = 379;
    long l380 = 380;
    long l381 = 381;
    long l382 = 382;
    long l383 = 383;
    long l384 = 384;
    long l385 = 385;
    long l386 = 386;
    long l387 = 387;
    long l388 = 388;
    long l389 = 389;
    long l390 = 390;
    long l391 = 391;
    long l392 = 392;
    long l393 = 393;
    long l394 = 394;
    long l395 = 395;
    long l396 = 396;
    long l397 = 397;
    long l398 = 398;
    long l399 = 399;
    long l400 = 400;
    long l401 = 401;
    long l402 = 402;
    long l403 = 403;
    long l404 = 404;
    long l405 = 405;
    long l406 = 406;
    long l407 = 407;
    long l408 = 408;
    long l409 = 409;
    long l410 = 410;
    long l411 = 411;
    long l412 = 412;
    long l413 = 413;
    long l414 = 414;
    long l415 = 415;
    long l416 = 416;
    long l417 = 417;
    long l418 = 418;
    long l419 = 419;
    long l420 = 420;
    long l421 = 421;
    long l422 = 422;
    long l423 = 423;
    long l424 = 424;
    long l425 = 425;
    long l426 = 426;
    long l427 = 427;
    long l428 = 428;
    long l429 = 429;
    long l430 = 430;
    long l431 = 431;
    long l432 = 432;
    long l433 = 433;
    long l434 = 434;
    long l435 = 435;
    long l436 = 436;
    long l437 = 437;
    long l438 = 438;
    long l439 = 439;
    long l440 = 440;
    long l441 = 441;
    long l442 = 442;
    long l443 = 443;
    long l444 = 444;
    long l445 = 445;
    long l446 = 446;
    long l447 = 447;
    long l448 = 448;
    long l449 = 449;
    long l450 = 450;
    long l451 = 451;
    long l452 = 452;
    long l453 = 453;
    long l454 = 454;
    long l455 = 455;
    long l456 = 456;
    long l457 = 457;
    long l458 = 458;
    long l459 = 459;
    long l460 = 460;
    long l461 = 461;
    long l462 = 462;
    long l463 = 463;
    long l464 = 464;
    long l465 = 465;
    long l466 = 466;
    long l467 = 467;
    long l468 = 468;
    long l469 = 469;
    long l470 = 470;
    long l471 = 471;
    long l472 = 472;
    long l473 = 473;
    long l474 = 474;
    long l475 = 475;
    long l476 = 476;
    long l477 = 477;
    long l478 = 478;
    long l479 = 479;
    long l480 = 480;
    long l481 = 481;
    long l482 = 482;
    long l483 = 483;
    long l484 = 484;
    long l485 = 485;
    long l486 = 486;
    long l487 = 487;
    long l488 = 488;
    long l489 = 489;
    long l490 = 490;
    long l491 = 491;
    long l492 = 492;
    long l493 = 493;
    long l494 = 494;
    long l495 = 495;
    long l496 = 496;
    long l497 = 497;
    long l498 = 498;
    long l499 = 499;
    long l500 = 500;
    long l501 = 501;
    long l502 = 502;
    long l503 = 503;
    long l504 = 504;
    long l505 = 505;
    long l506 = 506;
    long l507 = 507;
    long l508 = 508;
    long l509 = 509;
    long l510 = 510;
    long l511 = 511;
    long l512 = 512;
    long l513 = 513;
    long l514 = 514;
    long l515 = 515;
    long l516 = 516;
    long l517 = 517;
    long l518 = 518;
    long l519 = 519;
    long l520 = 520;
    long l521 = 521;
    long l522 = 522;
    long l523 = 523;
    long l524 = 524;
    long l525 = 525;
    long l526 = 526;
    long l527 = 527;
    long l528 = 528;
    long l529 = 529;
    long l530 = 530;
    long l531 = 531;
    long l532 = 532;
    long l533 = 533;
    long l534 = 534;
    long l535 = 535;
    long l536 = 536;
    long l537 = 537;
    long l538 = 538;
    long l539 = 539;
    long l540 = 540;
    long l541 = 541;
    long l542 = 542;
    long l543 = 543;
    long l544 = 544;
    long l545 = 545;
    long l546 = 546;
    long l547 = 547;
    long l548 = 548;
    long l549 = 549;
    long l550 = 550;
    long l551 = 551;
    long l552 = 552;
    long l553 = 553;
    long l554 = 554;
    long l555 = 555;
    long l556 = 556;
    long l557 = 557;
    long l558 = 558;
    long l559 = 559;
    long l560 = 560;
    long l561 = 561;
    long l562 = 562;
    long l563 = 563;
    long l564 = 564;
    long l565 = 565;
    long l566 = 566;
    long l567 = 567;
    long l568 = 568;
    long l569 = 569;
    long l570 = 570;
    long l571 = 571;
    long l572 = 572;
    long l573 = 573;
    long l574 = 574;
    long l575 = 575;
    long l576 = 576;
    long l577 = 577;
    long l578 = 578;
    long l579 = 579;
    long l580 = 580;
    long l581 = 581;
    long l582 = 582;
    long l583 = 583;
    long l584 = 584;
    long l585 = 585;
    long l586 = 586;
    long l587 = 587;
    long l588 = 588;
    long l589 = 589;
    long l590 = 590;
    long l591 = 591;
    long l592 = 592;
    long l593 = 593;
    long l594 = 594;
    long l595 = 595;
    long l596 = 596;
    long l597 = 597;
    long l598 = 598;
    long l599 = 599;
    long l600 = 600;
    long l601 = 601;
    long l602 = 602;
    long l603 = 603;
    long l604 = 604;
    long l605 = 605;
    long l606 = 606;
    long l607 = 607;
    long l608 = 608;
    long l609 = 609;
    long l610 = 610;
    long l611 = 611;
    long l612 = 612;
    long l613 = 613;
    long l614 = 614;
    long l615 = 615;
    long l616 = 616;
    long l617 = 617;
    long l618 = 618;
    long l619 = 619;
    long l620 = 620;
    long l621 = 621;
    long l622 = 622;
    long l623 = 623;
    long l624 = 624;
    long l625 = 625;
    long l626 = 626;
    long l627 = 627;
    long l628 = 628;
    long l629 = 629;
    long l630 = 630;
    long l631 = 631;
    long l632 = 632;
    long l633 = 633;
    long l634 = 634;
    long l635 = 635;
    long l636 = 636;
    long l637 = 637;
    long l638 = 638;
    long l639 = 639;
    long l640 = 640;
    long l641 = 641;
    long l642 = 642;
    long l643 = 643;
    long l644 = 644;
    long l645 = 645;
    long l646 = 646;
    long l647 = 647;
    long l648 = 648;
    long l649 = 649;
    long l650 = 650;
    long l651 = 651;
    long l652 = 652;
    long l653 = 653;
    long l654 = 654;
    long l655 = 655;
    long l656 = 656;
    long l657 = 657;
    long l658 = 658;
    long l659 = 659;
    long l660 = 660;
    long l661 = 661;
    long l662 = 662;
    long l663 = 663;
    long l664 = 664;
    long l665 = 665;
    long l666 = 666;
    long l667 = 667;
    long l668 = 668;
    long l669 = 669;
    long l670 = 670;
    long l671 = 671;
    long l672 = 672;
    long l673 = 673;
    long l674 = 674;
    long l675 = 675;
    long l676 = 676;
    long l677 = 677;
    long l678 = 678;
    long l679 = 679;
    long l680 = 680;
    long l681 = 681;
    long l682 = 682;
    long l683 = 683;
    long l684 = 684;
    long l685 = 685;
    long l686 = 686;
    long l687 = 687;
    long l688 = 688;
    long l689 = 689;
    long l690 = 690;
    long l691 = 691;
    long l692 = 692;
    long l693 = 693;
    long l694 = 694;
    long l695 = 695;
    long l696 = 696;
    long l697 = 697;
    long l698 = 698;
    long l699 = 699;
    long l700 = 700;
    long l701 = 701;
    long l702 = 702;
    long l703 = 703;
    long l704 = 704;
    long l705 = 705;
    long l706 = 706;
    long l707 = 707;
    long l708 = 708;
    long l709 = 709;
    long l710 = 710;
    long l711 = 711;
    long l712 = 712;
    long l713 = 713;
    long l714 = 714;
    long l715 = 715;
    long l716 = 716;
    long l717 = 717;
    long l718 = 718;
    long l719 = 719;
    long l720 = 720;
    long l721 = 721;
    long l722 = 722;
    long l723 = 723;
    long l724 = 724;
    long l725 = 725;
    long l726 = 726;
    long l727 = 727;
    long l728 = 728;
    long l729 = 729;
    long l730 = 730;
    long l731 = 731;
    long l732 = 732;
    long l733 = 733;
    long l734 = 734;
    long l735 = 735;
    long l736 = 736;
    long l737 = 737;
    long l738 = 738;
    long l739 = 739;
    long l740 = 740;
    long l741 = 741;
    long l742 = 742;
    long l743 = 743;
    long l744 = 744;
    long l745 = 745;
    long l746 = 746;
    long l747 = 747;
    long l748 = 748;
    long l749 = 749;
    long l750 = 750;
    long l751 = 751;
    long l752 = 752;
    long l753 = 753;
    long l754 = 754;
    long l755 = 755;
    long l756 = 756;
    long l757 = 757;
    long l758 = 758;
    long l759 = 759;
    long l760 = 760;
    long l761 = 761;
    long l762 = 762;
    long l763 = 763;
    long l764 = 764;
    long l765 = 765;
    long l766 = 766;
    long l767 = 767;
    long l768 = 768;
    long l769 = 769;
    long l770 = 770;
    long l771 = 771;
    long l772 = 772;
    long l773 = 773;
    long l774 = 774;
    long l775 = 775;
    long l776 = 776;
    long l777 = 777;
    long l778 = 778;
    long l779 = 779;
    long l780 = 780;
    long l781 = 781;
    long l782 = 782;
    long l783 = 783;
    long l784 = 784;
    long l785 = 785;
    long l786 = 786;
    long l787 = 787;
    long l788 = 788;
    long l789 = 789;
    long l790 = 790;
    long l791 = 791;
    long l792 = 792;
    long l793 = 793;
    long l794 = 794;
    long l795 = 795;
    long l796 = 796;
    long l797 = 797;
    long l798 = 798;
    long l799 = 799;
    long l800 = 800;
    long l801 = 801;
    long l802 = 802;
    long l803 = 803;
    long l804 = 804;
    long l805 = 805;
    long l806 = 806;
    long l807 = 807;
    long l808 = 808;
    long l809 = 809;
    long l810 = 810;
    long l811 = 811;
    long l812 = 812;
    long l813 = 813;
    long l814 = 814;
    long l815 = 815;
    long l816 = 816;
    long l817 = 817;
    long l818 = 818;
    long l819 = 819;
    long l820 = 820;
    long l821 = 821;
    long l822 = 822;
    long l823 = 823;
    long l824 = 824;
    long l825 = 825;
    long l826 = 826;
    long l827 = 827;
    long l828 = 828;
    long l829 = 829;
    long l830 = 830;
    long l831 = 831;
    long l832 = 832;
    long l833 = 833;
    long l834 = 834;
    long l835 = 835;
    long l836 = 836;
    long l837 = 837;
    long l838 = 838;
    long l839 = 839;
    long l840 = 840;
    long l841 = 841;
    long l842 = 842;
    long l843 = 843;
    long l844 = 844;
    long l845 = 845;
    long l846 = 846;
    long l847 = 847;
    long l848 = 848;
    long l849 = 849;
    long l850 = 850;
    long l851 = 851;
    long l852 = 852;
    long l853 = 853;
    long l854 = 854;
    long l855 = 855;
    long l856 = 856;
    long l857 = 857;
    long l858 = 858;
    long l859 = 859;
    long l860 = 860;
    long l861 = 861;
    long l862 = 862;
    long l863 = 863;
    long l864 = 864;
    long l865 = 865;
    long l866 = 866;
    long l867 = 867;
    long l868 = 868;
    long l869 = 869;
    long l870 = 870;
    long l871 = 871;
    long l872 = 872;
    long l873 = 873;
    long l874 = 874;
    long l875 = 875;
    long l876 = 876;
    long l877 = 877;
    long l878 = 878;
    long l879 = 879;
    long l880 = 880;
    long l881 = 881;
    long l882 = 882;
    long l883 = 883;
    long l884 = 884;
    long l885 = 885;
    long l886 = 886;
    long l887 = 887;
    long l888 = 888;
    long l889 = 889;
    long l890 = 890;
    long l891 = 891;
    long l892 = 892;
    long l893 = 893;
    long l894 = 894;
    long l895 = 895;
    long l896 = 896;
    long l897 = 897;
    long l898 = 898;
    long l899 = 899;
    long l900 = 900;
    long l901 = 901;
    long l902 = 902;
    long l903 = 903;
    long l904 = 904;
    long l905 = 905;
    long l906 = 906;
    long l907 = 907;
    long l908 = 908;
    long l909 = 909;
    long l910 = 910;
    long l911 = 911;
    long l912 = 912;
    long l913 = 913;
    long l914 = 914;
    long l915 = 915;
    long l916 = 916;
    long l917 = 917;
    long l918 = 918;
    long l919 = 919;
    long l920 = 920;
    long l921 = 921;
    long l922 = 922;
    long l923 = 923;
    long l924 = 924;
    long l925 = 925;
    long l926 = 926;
    long l927 = 927;
    long l928 = 928;
    long l929 = 929;
    long l930 = 930;
    long l931 = 931;
    long l932 = 932;
    long l933 = 933;
    long l934 = 934;
    long l935 = 935;
    long l936 = 936;
    long l937 = 937;
    long l938 = 938;
    long l939 = 939;
    long l940 = 940;
    long l941 = 941;
    long l942 = 942;
    long l943 = 943;
    long l944 = 944;
    long l945 = 945;
    long l946 = 946;
    long l947 = 947;
    long l948 = 948;
    long l949 = 949;
    long l950 = 950;
    long l951 = 951;
    long l952 = 952;
    long l953 = 953;
    long l954 = 954;
    long l955 = 955;
    long l956 = 956;
    long l957 = 957;
    long l958 = 958;
    long l959 = 959;
    long l960 = 960;
    long l961 = 961;
    long l962 = 962;
    long l963 = 963;
    long l964 = 964;
    long l965 = 965;
    long l966 = 966;
    long l967 = 967;
    long l968 = 968;
    long l969 = 969;
    long l970 = 970;
    long l971 = 971;
    long l972 = 972;
    long l973 = 973;
    long l974 = 974;
    long l975 = 975;
    long l976 = 976;
    long l977 = 977;
    long l978 = 978;
    long l979 = 979;
    long l980 = 980;
    long l981 = 981;
    long l982 = 982;
    long l983 = 983;
    long l984 = 984;
    long l985 = 985;
    long l986 = 986;
    long l987 = 987;
    long l988 = 988;
    long l989 = 989;
    long l990 = 990;
    long l991 = 991;
    long l992 = 992;
    long l993 = 993;
    long l994 = 994;
    long l995 = 995;
    long l996 = 996;
    long l997 = 997;
    long l998 = 998;
    long l999 = 999;
    l1 += l0;
    l2 += l1;
    l3 += l2;
    l4 += l3;
    l5 += l4;
    l6 += l5;
    l7 += l6;
    l8 += l7;
    l9 += l8;
    l10 += l9;
    l11 += l10;
    l12 += l11;
    l13 += l12;
    l14 += l13;
    l15 += l14;
    l16 += l15;
    l17 += l16;
    l18 += l17;
    l19 += l18;
    l20 += l19;
    l21 += l20;
    l22 += l21;
    l23 += l22;
    l24 += l23;
    l25 += l24;
    l26 += l25;
    l27 += l26;
    l28 += l27;
    l29 += l28;
    l30 += l29;
    l31 += l30;
    l32 += l31;
    l33 += l32;
    l34 += l33;
    l35 += l34;
    l36 += l35;
    l37 += l36;
    l38 += l37;
    l39 += l38;
    l40 += l39;
    l41 += l40;
    l42 += l41;
    l43 += l42;
    l44 += l43;
    l45 += l44;
    l46 += l45;
    l47 += l46;
    l48 += l47;
    l49 += l48;
    l50 += l49;
    l51 += l50;
    l52 += l51;
    l53 += l52;
    l54 += l53;
    l55 += l54;
    l56 += l55;
    l57 += l56;
    l58 += l57;
    l59 += l58;
    l60 += l59;
    l61 += l60;
    l62 += l61;
    l63 += l62;
    l64 += l63;
    l65 += l64;
    l66 += l65;
    l67 += l66;
    l68 += l67;
    l69 += l68;
    l70 += l69;
    l71 += l70;
    l72 += l71;
    l73 += l72;
    l74 += l73;
    l75 += l74;
    l76 += l75;
    l77 += l76;
    l78 += l77;
    l79 += l78;
    l80 += l79;
    l81 += l80;
    l82 += l81;
    l83 += l82;
    l84 += l83;
    l85 += l84;
    l86 += l85;
    l87 += l86;
    l88 += l87;
    l89 += l88;
    l90 += l89;
    l91 += l90;
    l92 += l91;
    l93 += l92;
    l94 += l93;
    l95 += l94;
    l96 += l95;
    l97 += l96;
    l98 += l97;
    l99 += l98;
    l100 += l99;
    l101 += l100;
    l102 += l101;
    l103 += l102;
    l104 += l103;
    l105 += l104;
    l106 += l105;
    l107 += l106;
    l108 += l107;
    l109 += l108;
    l110 += l109;
    l111 += l110;
    l112 += l111;
    l113 += l112;
    l114 += l113;
    l115 += l114;
    l116 += l115;
    l117 += l116;
    l118 += l117;
    l119 += l118;
    l120 += l119;
    l121 += l120;
    l122 += l121;
    l123 += l122;
    l124 += l123;
    l125 += l124;
    l126 += l125;
    l127 += l126;
    l128 += l127;
    l129 += l128;
    l130 += l129;
    l131 += l130;
    l132 += l131;
    l133 += l132;
    l134 += l133;
    l135 += l134;
    l136 += l135;
    l137 += l136;
    l138 += l137;
    l139 += l138;
    l140 += l139;
    l141 += l140;
    l142 += l141;
    l143 += l142;
    l144 += l143;
    l145 += l144;
    l146 += l145;
    l147 += l146;
    l148 += l147;
    l149 += l148;
    l150 += l149;
    l151 += l150;
    l152 += l151;
    l153 += l152;
    l154 += l153;
    l155 += l154;
    l156 += l155;
    l157 += l156;
    l158 += l157;
    l159 += l158;
    l160 += l159;
    l161 += l160;
    l162 += l161;
    l163 += l162;
    l164 += l163;
    l165 += l164;
    l166 += l165;
    l167 += l166;
    l168 += l167;
    l169 += l168;
    l170 += l169;
    l171 += l170;
    l172 += l171;
    l173 += l172;
    l174 += l173;
    l175 += l174;
    l176 += l175;
    l177 += l176;
    l178 += l177;
    l179 += l178;
    l180 += l179;
    l181 += l180;
    l182 += l181;
    l183 += l182;
    l184 += l183;
    l185 += l184;
    l186 += l185;
    l187 += l186;
    l188 += l187;
    l189 += l188;
    l190 += l189;
    l191 += l190;
    l192 += l191;
    l193 += l192;
    l194 += l193;
    l195 += l194;
    l196 += l195;
    l197 += l196;
    l198 += l197;
    l199 += l198;
    l200 += l199;
    l201 += l200;
    l202 += l201;
    l203 += l202;
    l204 += l203;
    l205 += l204;
    l206 += l205;
    l207 += l206;
    l208 += l207;
    l209 += l208;
    l210 += l209;
    l211 += l210;
    l212 += l211;
    l213 += l212;
    l214 += l213;
    l215 += l214;
    l216 += l215;
    l217 += l216;
    l218 += l217;
    l219 += l218;
    l220 += l219;
    l221 += l220;
    l222 += l221;
    l223 += l222;
    l224 += l223;
    l225 += l224;
    l226 += l225;
    l227 += l226;
    l228 += l227;
    l229 += l228;
    l230 += l229;
    l231 += l230;
    l232 += l231;
    l233 += l232;
    l234 += l233;
    l235 += l234;
    l236 += l235;
    l237 += l236;
    l238 += l237;
    l239 += l238;
    l240 += l239;
    l241 += l240;
    l242 += l241;
    l243 += l242;
    l244 += l243;
    l245 += l244;
    l246 += l245;
    l247 += l246;
    l248 += l247;
    l249 += l248;
    l250 += l249;
    l251 += l250;
    l252 += l251;
    l253 += l252;
    l254 += l253;
    l255 += l254;
    l256 += l255;
    l257 += l256;
    l258 += l257;
    l259 += l258;
    l260 += l259;
    l261 += l260;
    l262 += l261;
    l263 += l262;
    l264 += l263;
    l265 += l264;
    l266 += l265;
    l267 += l266;
    l268 += l267;
    l269 += l268;
    l270 += l269;
    l271 += l270;
    l272 += l271;
    l273 += l272;
    l274 += l273;
    l275 += l274;
    l276 += l275;
    l277 += l276;
    l278 += l277;
    l279 += l278;
    l280 += l279;
    l281 += l280;
    l282 += l281;
    l283 += l282;
    l284 += l283;
    l285 += l284;
    l286 += l285;
    l287 += l286;
    l288 += l287;
    l289 += l288;
    l290 += l289;
    l291 += l290;
    l292 += l291;
    l293 += l292;
    l294 += l293;
    l295 += l294;
    l296 += l295;
    l297 += l296;
    l298 += l297;
    l299 += l298;
    l300 += l299;
    l301 += l300;
    l302 += l301;
    l303 += l302;
    l304 += l303;
    l305 += l304;
    l306 += l305;
    l307 += l306;
    l308 += l307;
    l309 += l308;
    l310 += l309;
    l311 += l310;
    l312 += l311;
    l313 += l312;
    l314 += l313;
    l315 += l314;
    l316 += l315;
    l317 += l316;
    l318 += l317;
    l319 += l318;
    l320 += l319;
    l321 += l320;
    l322 += l321;
    l323 += l322;
    l324 += l323;
    l325 += l324;
    l326 += l325;
    l327 += l326;
    l328 += l327;
    l329 += l328;
    l330 += l329;
    l331 += l330;
    l332 += l331;
    l333 += l332;
    l334 += l333;
    l335 += l334;
    l336 += l335;
    l337 += l336;
    l338 += l337;
    l339 += l338;
    l340 += l339;
    l341 += l340;
    l342 += l341;
    l343 += l342;
    l344 += l343;
    l345 += l344;
    l346 += l345;
    l347 += l346;
    l348 += l347;
    l349 += l348;
    l350 += l349;
    l351 += l350;
    l352 += l351;
    l353 += l352;
    l354 += l353;
    l355 += l354;
    l356 += l355;
    l357 += l356;
    l358 += l357;
    l359 += l358;
    l360 += l359;
    l361 += l360;
    l362 += l361;
    l363 += l362;
    l364 += l363;
    l365 += l364;
    l366 += l365;
    l367 += l366;
    l368 += l367;
    l369 += l368;
    l370 += l369;
    l371 += l370;
    l372 += l371;
    l373 += l372;
    l374 += l373;
    l375 += l374;
    l376 += l375;
    l377 += l376;
    l378 += l377;
    l379 += l378;
    l380 += l379;
    l381 += l380;
    l382 += l381;
    l383 += l382;
    l384 += l383;
    l385 += l384;
    l386 += l385;
    l387 += l386;
    l388 += l387;
    l389 += l388;
    l390 += l389;
    l391 += l390;
    l392 += l391;
    l393 += l392;
    l394 += l393;
    l395 += l394;
    l396 += l395;
    l397 += l396;
    l398 += l397;
    l399 += l398;
    l400 += l399;
    l401 += l400;
    l402 += l401;
    l403 += l402;
    l404 += l403;
    l405 += l404;
    l406 += l405;
    l407 += l406;
    l408 += l407;
    l409 += l408;
    l410 += l409;
    l411 += l410;
    l412 += l411;
    l413 += l412;
    l414 += l413;
    l415 += l414;
    l416 += l415;
    l417 += l416;
    l418 += l417;
    l419 += l418;
    l420 += l419;
    l421 += l420;
    l422 += l421;
    l423 += l422;
    l424 += l423;
    l425 += l424;
    l426 += l425;
    l427 += l426;
    l428 += l427;
    l429 += l428;
    l430 += l429;
    l431 += l430;
    l432 += l431;
    l433 += l432;
    l434 += l433;
    l435 += l434;
    l436 += l435;
    l437 += l436;
    l438 += l437;
    l439 += l438;
    l440 += l439;
    l441 += l440;
    l442 += l441;
    l443 += l442;
    l444 += l443;
    l445 += l444;
    l446 += l445;
    l447 += l446;
    l448 += l447;
    l449 += l448;
    l450 += l449;
    l451 += l450;
    l452 += l451;
    l453 += l452;
    l454 += l453;
    l455 += l454;
    l456 += l455;
    l457 += l456;
    l458 += l457;
    l459 += l458;
    l460 += l459;
    l461 += l460;
    l462 += l461;
    l463 += l462;
    l464 += l463;
    l465 += l464;
    l466 += l465;
    l467 += l466;
    l468 += l467;
    l469 += l468;
    l470 += l469;
    l471 += l470;
    l472 += l471;
    l473 += l472;
    l474 += l473;
    l475 += l474;
    l476 += l475;
    l477 += l476;
    l478 += l477;
    l479 += l478;
    l480 += l479;
    l481 += l480;
    l482 += l481;
    l483 += l482;
    l484 += l483;
    l485 += l484;
    l486 += l485;
    l487 += l486;
    l488 += l487;
    l489 += l488;
    l490 += l489;
    l491 += l490;
    l492 += l491;
    l493 += l492;
    l494 += l493;
    l495 += l494;
    l496 += l495;
    l497 += l496;
    l498 += l497;
    l499 += l498;
    l500 += l499;
    l501 += l500;
    l502 += l501;
    l503 += l502;
    l504 += l503;
    l505 += l504;
    l506 += l505;
    l507 += l506;
    l508 += l507;
    l509 += l508;
    l510 += l509;
    l511 += l510;
    l512 += l511;
    l513 += l512;
    l514 += l513;
    l515 += l514;
    l516 += l515;
    l517 += l516;
    l518 += l517;
    l519 += l518;
    l520 += l519;
    l521 += l520;
    l522 += l521;
    l523 += l522;
    l524 += l523;
    l525 += l524;
    l526 += l525;
    l527 += l526;
    l528 += l527;
    l529 += l528;
    l530 += l529;
    l531 += l530;
    l532 += l531;
    l533 += l532;
    l534 += l533;
    l535 += l534;
    l536 += l535;
    l537 += l536;
    l538 += l537;
    l539 += l538;
    l540 += l539;
    l541 += l540;
    l542 += l541;
    l543 += l542;
    l544 += l543;
    l545 += l544;
    l546 += l545;
    l547 += l546;
    l548 += l547;
    l549 += l548;
    l550 += l549;
    l551 += l550;
    l552 += l551;
    l553 += l552;
    l554 += l553;
    l555 += l554;
    l556 += l555;
    l557 += l556;
    l558 += l557;
    l559 += l558;
    l560 += l559;
    l561 += l560;
    l562 += l561;
    l563 += l562;
    l564 += l563;
    l565 += l564;
    l566 += l565;
    l567 += l566;
    l568 += l567;
    l569 += l568;
    l570 += l569;
    l571 += l570;
    l572 += l571;
    l573 += l572;
    l574 += l573;
    l575 += l574;
    l576 += l575;
    l577 += l576;
    l578 += l577;
    l579 += l578;
    l580 += l579;
    l581 += l580;
    l582 += l581;
    l583 += l582;
    l584 += l583;
    l585 += l584;
    l586 += l585;
    l587 += l586;
    l588 += l587;
    l589 += l588;
    l590 += l589;
    l591 += l590;
    l592 += l591;
    l593 += l592;
    l594 += l593;
    l595 += l594;
    l596 += l595;
    l597 += l596;
    l598 += l597;
    l599 += l598;
    l600 += l599;
    l601 += l600;
    l602 += l601;
    l603 += l602;
    l604 += l603;
    l605 += l604;
    l606 += l605;
    l607 += l606;
    l608 += l607;
    l609 += l608;
    l610 += l609;
    l611 += l610;
    l612 += l611;
    l613 += l612;
    l614 += l613;
    l615 += l614;
    l616 += l615;
    l617 += l616;
    l618 += l617;
    l619 += l618;
    l620 += l619;
    l621 += l620;
    l622 += l621;
    l623 += l622;
    l624 += l623;
    l625 += l624;
    l626 += l625;
    l627 += l626;
    l628 += l627;
    l629 += l628;
    l630 += l629;
    l631 += l630;
    l632 += l631;
    l633 += l632;
    l634 += l633;
    l635 += l634;
    l636 += l635;
    l637 += l636;
    l638 += l637;
    l639 += l638;
    l640 += l639;
    l641 += l640;
    l642 += l641;
    l643 += l642;
    l644 += l643;
    l645 += l644;
    l646 += l645;
    l647 += l646;
    l648 += l647;
    l649 += l648;
    l650 += l649;
    l651 += l650;
    l652 += l651;
    l653 += l652;
    l654 += l653;
    l655 += l654;
    l656 += l655;
    l657 += l656;
    l658 += l657;
    l659 += l658;
    l660 += l659;
    l661 += l660;
    l662 += l661;
    l663 += l662;
    l664 += l663;
    l665 += l664;
    l666 += l665;
    l667 += l666;
    l668 += l667;
    l669 += l668;
    l670 += l669;
    l671 += l670;
    l672 += l671;
    l673 += l672;
    l674 += l673;
    l675 += l674;
    l676 += l675;
    l677 += l676;
    l678 += l677;
    l679 += l678;
    l680 += l679;
    l681 += l680;
    l682 += l681;
    l683 += l682;
    l684 += l683;
    l685 += l684;
    l686 += l685;
    l687 += l686;
    l688 += l687;
    l689 += l688;
    l690 += l689;
    l691 += l690;
    l692 += l691;
    l693 += l692;
    l694 += l693;
    l695 += l694;
    l696 += l695;
    l697 += l696;
    l698 += l697;
    l699 += l698;
    l700 += l699;
    l701 += l700;
    l702 += l701;
    l703 += l702;
    l704 += l703;
    l705 += l704;
    l706 += l705;
    l707 += l706;
    l708 += l707;
    l709 += l708;
    l710 += l709;
    l711 += l710;
    l712 += l711;
    l713 += l712;
    l714 += l713;
    l715 += l714;
    l716 += l715;
    l717 += l716;
    l718 += l717;
    l719 += l718;
    l720 += l719;
    l721 += l720;
    l722 += l721;
    l723 += l722;
    l724 += l723;
    l725 += l724;
    l726 += l725;
    l727 += l726;
    l728 += l727;
    l729 += l728;
    l730 += l729;
    l731 += l730;
    l732 += l731;
    l733 += l732;
    l734 += l733;
    l735 += l734;
    l736 += l735;
    l737 += l736;
    l738 += l737;
    l739 += l738;
    l740 += l739;
    l741 += l740;
    l742 += l741;
    l743 += l742;
    l744 += l743;
    l745 += l744;
    l746 += l745;
    l747 += l746;
    l748 += l747;
    l749 += l748;
    l750 += l749;
    l751 += l750;
    l752 += l751;
    l753 += l752;
    l754 += l753;
    l755 += l754;
    l756 += l755;
    l757 += l756;
    l758 += l757;
    l759 += l758;
    l760 += l759;
    l761 += l760;
    l762 += l761;
    l763 += l762;
    l764 += l763;
    l765 += l764;
    l766 += l765;
    l767 += l766;
    l768 += l767;
    l769 += l768;
    l770 += l769;
    l771 += l770;
    l772 += l771;
    l773 += l772;
    l774 += l773;
    l775 += l774;
    l776 += l775;
    l777 += l776;
    l778 += l777;
    l779 += l778;
    l780 += l779;
    l781 += l780;
    l782 += l781;
    l783 += l782;
    l784 += l783;
    l785 += l784;
    l786 += l785;
    l787 += l786;
    l788 += l787;
    l789 += l788;
    l790 += l789;
    l791 += l790;
    l792 += l791;
    l793 += l792;
    l794 += l793;
    l795 += l794;
    l796 += l795;
    l797 += l796;
    l798 += l797;
    l799 += l798;
    l800 += l799;
    l801 += l800;
    l802 += l801;
    l803 += l802;
    l804 += l803;
    l805 += l804;
    l806 += l805;
    l807 += l806;
    l808 += l807;
    l809 += l808;
    l810 += l809;
    l811 += l810;
    l812 += l811;
    l813 += l812;
    l814 += l813;
    l815 += l814;
    l816 += l815;
    l817 += l816;
    l818 += l817;
    l819 += l818;
    l820 += l819;
    l821 += l820;
    l822 += l821;
    l823 += l822;
    l824 += l823;
    l825 += l824;
    l826 += l825;
    l827 += l826;
    l828 += l827;
    l829 += l828;
    l830 += l829;
    l831 += l830;
    l832 += l831;
    l833 += l832;
    l834 += l833;
    l835 += l834;
    l836 += l835;
    l837 += l836;
    l838 += l837;
    l839 += l838;
    l840 += l839;
    l841 += l840;
    l842 += l841;
    l843 += l842;
    l844 += l843;
    l845 += l844;
    l846 += l845;
    l847 += l846;
    l848 += l847;
    l849 += l848;
    l850 += l849;
    l851 += l850;
    l852 += l851;
    l853 += l852;
    l854 += l853;
    l855 += l854;
    l856 += l855;
    l857 += l856;
    l858 += l857;
    l859 += l858;
    l860 += l859;
    l861 += l860;
    l862 += l861;
    l863 += l862;
    l864 += l863;
    l865 += l864;
    l866 += l865;
    l867 += l866;
    l868 += l867;
    l869 += l868;
    l870 += l869;
    l871 += l870;
    l872 += l871;
    l873 += l872;
    l874 += l873;
    l875 += l874;
    l876 += l875;
    l877 += l876;
    l878 += l877;
    l879 += l878;
    l880 += l879;
    l881 += l880;
    l882 += l881;
    l883 += l882;
    l884 += l883;
    l885 += l884;
    l886 += l885;
    l887 += l886;
    l888 += l887;
    l889 += l888;
    l890 += l889;
    l891 += l890;
    l892 += l891;
    l893 += l892;
    l894 += l893;
    l895 += l894;
    l896 += l895;
    l897 += l896;
    l898 += l897;
    l899 += l898;
    l900 += l899;
    l901 += l900;
    l902 += l901;
    l903 += l902;
    l904 += l903;
    l905 += l904;
    l906 += l905;
    l907 += l906;
    l908 += l907;
    l909 += l908;
    l910 += l909;
    l911 += l910;
    l912 += l911;
    l913 += l912;
    l914 += l913;
    l915 += l914;
    l916 += l915;
    l917 += l916;
    l918 += l917;
    l919 += l918;
    l920 += l919;
    l921 += l920;
    l922 += l921;
    l923 += l922;
    l924 += l923;
    l925 += l924;
    l926 += l925;
    l927 += l926;
    l928 += l927;
    l929 += l928;
    l930 += l929;
    l931 += l930;
    l932 += l931;
    l933 += l932;
    l934 += l933;
    l935 += l934;
    l936 += l935;
    l937 += l936;
    l938 += l937;
    l939 += l938;
    l940 += l939;
    l941 += l940;
    l942 += l941;
    l943 += l942;
    l944 += l943;
    l945 += l944;
    l946 += l945;
    l947 += l946;
    l948 += l947;
    l949 += l948;
    l950 += l949;
    l951 += l950;
    l952 += l951;
    l953 += l952;
    l954 += l953;
    l955 += l954;
    l956 += l955;
    l957 += l956;
    l958 += l957;
    l959 += l958;
    l960 += l959;
    l961 += l960;
    l962 += l961;
    l963 += l962;
    l964 += l963;
    l965 += l964;
    l966 += l965;
    l967 += l966;
    l968 += l967;
    l969 += l968;
    l970 += l969;
    l971 += l970;
    l972 += l971;
    l973 += l972;
    l974 += l973;
    l975 += l974;
    l976 += l975;
    l977 += l976;
    l978 += l977;
    l979 += l978;
    l980 += l979;
    l981 += l980;
    l982 += l981;
    l983 += l982;
    l984 += l983;
    l985 += l984;
    l986 += l985;
    l987 += l986;
    l988 += l987;
    l989 += l988;
    l990 += l989;
    l991 += l990;
    l992 += l991;
    l993 += l992;
    l994 += l993;
    l995 += l994;
    l996 += l995;
    l997 += l996;
    l998 += l997;
    l999 += l998;
    // Create a branch to beat the large method check.
    if (l998 == l999) {
      return l998;
    } else {
      return l999;
    }
  }
}
