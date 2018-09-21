/*
 * Copyright (C) 2018 The Android Open Source Project
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

  public static class TargetClass {
    public void foo() {}
  }

  // Stress testing class-loading by jit. We found that 250 classes triggers the jit loading a class
  // fairly consistently.
  public static class TC0 { public static final int FOO; static { FOO = 1; } }
  public static class TC1 { public static final int FOO; static { FOO = 2; } }
  public static class TC2 { public static final int FOO; static { FOO = 3; } }
  public static class TC3 { public static final int FOO; static { FOO = 4; } }
  public static class TC4 { public static final int FOO; static { FOO = 5; } }
  public static class TC5 { public static final int FOO; static { FOO = 6; } }
  public static class TC6 { public static final int FOO; static { FOO = 7; } }
  public static class TC7 { public static final int FOO; static { FOO = 8; } }
  public static class TC8 { public static final int FOO; static { FOO = 9; } }
  public static class TC9 { public static final int FOO; static { FOO = 10; } }
  public static class TC10 { public static final int FOO; static { FOO = 11; } }
  public static class TC11 { public static final int FOO; static { FOO = 12; } }
  public static class TC12 { public static final int FOO; static { FOO = 13; } }
  public static class TC13 { public static final int FOO; static { FOO = 14; } }
  public static class TC14 { public static final int FOO; static { FOO = 15; } }
  public static class TC15 { public static final int FOO; static { FOO = 16; } }
  public static class TC16 { public static final int FOO; static { FOO = 17; } }
  public static class TC17 { public static final int FOO; static { FOO = 18; } }
  public static class TC18 { public static final int FOO; static { FOO = 19; } }
  public static class TC19 { public static final int FOO; static { FOO = 20; } }
  public static class TC20 { public static final int FOO; static { FOO = 21; } }
  public static class TC21 { public static final int FOO; static { FOO = 22; } }
  public static class TC22 { public static final int FOO; static { FOO = 23; } }
  public static class TC23 { public static final int FOO; static { FOO = 24; } }
  public static class TC24 { public static final int FOO; static { FOO = 25; } }
  public static class TC25 { public static final int FOO; static { FOO = 26; } }
  public static class TC26 { public static final int FOO; static { FOO = 27; } }
  public static class TC27 { public static final int FOO; static { FOO = 28; } }
  public static class TC28 { public static final int FOO; static { FOO = 29; } }
  public static class TC29 { public static final int FOO; static { FOO = 30; } }
  public static class TC30 { public static final int FOO; static { FOO = 31; } }
  public static class TC31 { public static final int FOO; static { FOO = 32; } }
  public static class TC32 { public static final int FOO; static { FOO = 33; } }
  public static class TC33 { public static final int FOO; static { FOO = 34; } }
  public static class TC34 { public static final int FOO; static { FOO = 35; } }
  public static class TC35 { public static final int FOO; static { FOO = 36; } }
  public static class TC36 { public static final int FOO; static { FOO = 37; } }
  public static class TC37 { public static final int FOO; static { FOO = 38; } }
  public static class TC38 { public static final int FOO; static { FOO = 39; } }
  public static class TC39 { public static final int FOO; static { FOO = 40; } }
  public static class TC40 { public static final int FOO; static { FOO = 41; } }
  public static class TC41 { public static final int FOO; static { FOO = 42; } }
  public static class TC42 { public static final int FOO; static { FOO = 43; } }
  public static class TC43 { public static final int FOO; static { FOO = 44; } }
  public static class TC44 { public static final int FOO; static { FOO = 45; } }
  public static class TC45 { public static final int FOO; static { FOO = 46; } }
  public static class TC46 { public static final int FOO; static { FOO = 47; } }
  public static class TC47 { public static final int FOO; static { FOO = 48; } }
  public static class TC48 { public static final int FOO; static { FOO = 49; } }
  public static class TC49 { public static final int FOO; static { FOO = 50; } }
  public static class TC50 { public static final int FOO; static { FOO = 51; } }
  public static class TC51 { public static final int FOO; static { FOO = 52; } }
  public static class TC52 { public static final int FOO; static { FOO = 53; } }
  public static class TC53 { public static final int FOO; static { FOO = 54; } }
  public static class TC54 { public static final int FOO; static { FOO = 55; } }
  public static class TC55 { public static final int FOO; static { FOO = 56; } }
  public static class TC56 { public static final int FOO; static { FOO = 57; } }
  public static class TC57 { public static final int FOO; static { FOO = 58; } }
  public static class TC58 { public static final int FOO; static { FOO = 59; } }
  public static class TC59 { public static final int FOO; static { FOO = 60; } }
  public static class TC60 { public static final int FOO; static { FOO = 61; } }
  public static class TC61 { public static final int FOO; static { FOO = 62; } }
  public static class TC62 { public static final int FOO; static { FOO = 63; } }
  public static class TC63 { public static final int FOO; static { FOO = 64; } }
  public static class TC64 { public static final int FOO; static { FOO = 65; } }
  public static class TC65 { public static final int FOO; static { FOO = 66; } }
  public static class TC66 { public static final int FOO; static { FOO = 67; } }
  public static class TC67 { public static final int FOO; static { FOO = 68; } }
  public static class TC68 { public static final int FOO; static { FOO = 69; } }
  public static class TC69 { public static final int FOO; static { FOO = 70; } }
  public static class TC70 { public static final int FOO; static { FOO = 71; } }
  public static class TC71 { public static final int FOO; static { FOO = 72; } }
  public static class TC72 { public static final int FOO; static { FOO = 73; } }
  public static class TC73 { public static final int FOO; static { FOO = 74; } }
  public static class TC74 { public static final int FOO; static { FOO = 75; } }
  public static class TC75 { public static final int FOO; static { FOO = 76; } }
  public static class TC76 { public static final int FOO; static { FOO = 77; } }
  public static class TC77 { public static final int FOO; static { FOO = 78; } }
  public static class TC78 { public static final int FOO; static { FOO = 79; } }
  public static class TC79 { public static final int FOO; static { FOO = 80; } }
  public static class TC80 { public static final int FOO; static { FOO = 81; } }
  public static class TC81 { public static final int FOO; static { FOO = 82; } }
  public static class TC82 { public static final int FOO; static { FOO = 83; } }
  public static class TC83 { public static final int FOO; static { FOO = 84; } }
  public static class TC84 { public static final int FOO; static { FOO = 85; } }
  public static class TC85 { public static final int FOO; static { FOO = 86; } }
  public static class TC86 { public static final int FOO; static { FOO = 87; } }
  public static class TC87 { public static final int FOO; static { FOO = 88; } }
  public static class TC88 { public static final int FOO; static { FOO = 89; } }
  public static class TC89 { public static final int FOO; static { FOO = 90; } }
  public static class TC90 { public static final int FOO; static { FOO = 91; } }
  public static class TC91 { public static final int FOO; static { FOO = 92; } }
  public static class TC92 { public static final int FOO; static { FOO = 93; } }
  public static class TC93 { public static final int FOO; static { FOO = 94; } }
  public static class TC94 { public static final int FOO; static { FOO = 95; } }
  public static class TC95 { public static final int FOO; static { FOO = 96; } }
  public static class TC96 { public static final int FOO; static { FOO = 97; } }
  public static class TC97 { public static final int FOO; static { FOO = 98; } }
  public static class TC98 { public static final int FOO; static { FOO = 99; } }
  public static class TC99 { public static final int FOO; static { FOO = 100; } }
  public static class TC100 { public static final int FOO; static { FOO = 101; } }
  public static class TC101 { public static final int FOO; static { FOO = 102; } }
  public static class TC102 { public static final int FOO; static { FOO = 103; } }
  public static class TC103 { public static final int FOO; static { FOO = 104; } }
  public static class TC104 { public static final int FOO; static { FOO = 105; } }
  public static class TC105 { public static final int FOO; static { FOO = 106; } }
  public static class TC106 { public static final int FOO; static { FOO = 107; } }
  public static class TC107 { public static final int FOO; static { FOO = 108; } }
  public static class TC108 { public static final int FOO; static { FOO = 109; } }
  public static class TC109 { public static final int FOO; static { FOO = 110; } }
  public static class TC110 { public static final int FOO; static { FOO = 111; } }
  public static class TC111 { public static final int FOO; static { FOO = 112; } }
  public static class TC112 { public static final int FOO; static { FOO = 113; } }
  public static class TC113 { public static final int FOO; static { FOO = 114; } }
  public static class TC114 { public static final int FOO; static { FOO = 115; } }
  public static class TC115 { public static final int FOO; static { FOO = 116; } }
  public static class TC116 { public static final int FOO; static { FOO = 117; } }
  public static class TC117 { public static final int FOO; static { FOO = 118; } }
  public static class TC118 { public static final int FOO; static { FOO = 119; } }
  public static class TC119 { public static final int FOO; static { FOO = 120; } }
  public static class TC120 { public static final int FOO; static { FOO = 121; } }
  public static class TC121 { public static final int FOO; static { FOO = 122; } }
  public static class TC122 { public static final int FOO; static { FOO = 123; } }
  public static class TC123 { public static final int FOO; static { FOO = 124; } }
  public static class TC124 { public static final int FOO; static { FOO = 125; } }
  public static class TC125 { public static final int FOO; static { FOO = 126; } }
  public static class TC126 { public static final int FOO; static { FOO = 127; } }
  public static class TC127 { public static final int FOO; static { FOO = 128; } }
  public static class TC128 { public static final int FOO; static { FOO = 129; } }
  public static class TC129 { public static final int FOO; static { FOO = 130; } }
  public static class TC130 { public static final int FOO; static { FOO = 131; } }
  public static class TC131 { public static final int FOO; static { FOO = 132; } }
  public static class TC132 { public static final int FOO; static { FOO = 133; } }
  public static class TC133 { public static final int FOO; static { FOO = 134; } }
  public static class TC134 { public static final int FOO; static { FOO = 135; } }
  public static class TC135 { public static final int FOO; static { FOO = 136; } }
  public static class TC136 { public static final int FOO; static { FOO = 137; } }
  public static class TC137 { public static final int FOO; static { FOO = 138; } }
  public static class TC138 { public static final int FOO; static { FOO = 139; } }
  public static class TC139 { public static final int FOO; static { FOO = 140; } }
  public static class TC140 { public static final int FOO; static { FOO = 141; } }
  public static class TC141 { public static final int FOO; static { FOO = 142; } }
  public static class TC142 { public static final int FOO; static { FOO = 143; } }
  public static class TC143 { public static final int FOO; static { FOO = 144; } }
  public static class TC144 { public static final int FOO; static { FOO = 145; } }
  public static class TC145 { public static final int FOO; static { FOO = 146; } }
  public static class TC146 { public static final int FOO; static { FOO = 147; } }
  public static class TC147 { public static final int FOO; static { FOO = 148; } }
  public static class TC148 { public static final int FOO; static { FOO = 149; } }
  public static class TC149 { public static final int FOO; static { FOO = 150; } }
  public static class TC150 { public static final int FOO; static { FOO = 151; } }
  public static class TC151 { public static final int FOO; static { FOO = 152; } }
  public static class TC152 { public static final int FOO; static { FOO = 153; } }
  public static class TC153 { public static final int FOO; static { FOO = 154; } }
  public static class TC154 { public static final int FOO; static { FOO = 155; } }
  public static class TC155 { public static final int FOO; static { FOO = 156; } }
  public static class TC156 { public static final int FOO; static { FOO = 157; } }
  public static class TC157 { public static final int FOO; static { FOO = 158; } }
  public static class TC158 { public static final int FOO; static { FOO = 159; } }
  public static class TC159 { public static final int FOO; static { FOO = 160; } }
  public static class TC160 { public static final int FOO; static { FOO = 161; } }
  public static class TC161 { public static final int FOO; static { FOO = 162; } }
  public static class TC162 { public static final int FOO; static { FOO = 163; } }
  public static class TC163 { public static final int FOO; static { FOO = 164; } }
  public static class TC164 { public static final int FOO; static { FOO = 165; } }
  public static class TC165 { public static final int FOO; static { FOO = 166; } }
  public static class TC166 { public static final int FOO; static { FOO = 167; } }
  public static class TC167 { public static final int FOO; static { FOO = 168; } }
  public static class TC168 { public static final int FOO; static { FOO = 169; } }
  public static class TC169 { public static final int FOO; static { FOO = 170; } }
  public static class TC170 { public static final int FOO; static { FOO = 171; } }
  public static class TC171 { public static final int FOO; static { FOO = 172; } }
  public static class TC172 { public static final int FOO; static { FOO = 173; } }
  public static class TC173 { public static final int FOO; static { FOO = 174; } }
  public static class TC174 { public static final int FOO; static { FOO = 175; } }
  public static class TC175 { public static final int FOO; static { FOO = 176; } }
  public static class TC176 { public static final int FOO; static { FOO = 177; } }
  public static class TC177 { public static final int FOO; static { FOO = 178; } }
  public static class TC178 { public static final int FOO; static { FOO = 179; } }
  public static class TC179 { public static final int FOO; static { FOO = 180; } }
  public static class TC180 { public static final int FOO; static { FOO = 181; } }
  public static class TC181 { public static final int FOO; static { FOO = 182; } }
  public static class TC182 { public static final int FOO; static { FOO = 183; } }
  public static class TC183 { public static final int FOO; static { FOO = 184; } }
  public static class TC184 { public static final int FOO; static { FOO = 185; } }
  public static class TC185 { public static final int FOO; static { FOO = 186; } }
  public static class TC186 { public static final int FOO; static { FOO = 187; } }
  public static class TC187 { public static final int FOO; static { FOO = 188; } }
  public static class TC188 { public static final int FOO; static { FOO = 189; } }
  public static class TC189 { public static final int FOO; static { FOO = 190; } }
  public static class TC190 { public static final int FOO; static { FOO = 191; } }
  public static class TC191 { public static final int FOO; static { FOO = 192; } }
  public static class TC192 { public static final int FOO; static { FOO = 193; } }
  public static class TC193 { public static final int FOO; static { FOO = 194; } }
  public static class TC194 { public static final int FOO; static { FOO = 195; } }
  public static class TC195 { public static final int FOO; static { FOO = 196; } }
  public static class TC196 { public static final int FOO; static { FOO = 197; } }
  public static class TC197 { public static final int FOO; static { FOO = 198; } }
  public static class TC198 { public static final int FOO; static { FOO = 199; } }
  public static class TC199 { public static final int FOO; static { FOO = 200; } }
  public static class TC200 { public static final int FOO; static { FOO = 201; } }
  public static class TC201 { public static final int FOO; static { FOO = 202; } }
  public static class TC202 { public static final int FOO; static { FOO = 203; } }
  public static class TC203 { public static final int FOO; static { FOO = 204; } }
  public static class TC204 { public static final int FOO; static { FOO = 205; } }
  public static class TC205 { public static final int FOO; static { FOO = 206; } }
  public static class TC206 { public static final int FOO; static { FOO = 207; } }
  public static class TC207 { public static final int FOO; static { FOO = 208; } }
  public static class TC208 { public static final int FOO; static { FOO = 209; } }
  public static class TC209 { public static final int FOO; static { FOO = 210; } }
  public static class TC210 { public static final int FOO; static { FOO = 211; } }
  public static class TC211 { public static final int FOO; static { FOO = 212; } }
  public static class TC212 { public static final int FOO; static { FOO = 213; } }
  public static class TC213 { public static final int FOO; static { FOO = 214; } }
  public static class TC214 { public static final int FOO; static { FOO = 215; } }
  public static class TC215 { public static final int FOO; static { FOO = 216; } }
  public static class TC216 { public static final int FOO; static { FOO = 217; } }
  public static class TC217 { public static final int FOO; static { FOO = 218; } }
  public static class TC218 { public static final int FOO; static { FOO = 219; } }
  public static class TC219 { public static final int FOO; static { FOO = 220; } }
  public static class TC220 { public static final int FOO; static { FOO = 221; } }
  public static class TC221 { public static final int FOO; static { FOO = 222; } }
  public static class TC222 { public static final int FOO; static { FOO = 223; } }
  public static class TC223 { public static final int FOO; static { FOO = 224; } }
  public static class TC224 { public static final int FOO; static { FOO = 225; } }
  public static class TC225 { public static final int FOO; static { FOO = 226; } }
  public static class TC226 { public static final int FOO; static { FOO = 227; } }
  public static class TC227 { public static final int FOO; static { FOO = 228; } }
  public static class TC228 { public static final int FOO; static { FOO = 229; } }
  public static class TC229 { public static final int FOO; static { FOO = 230; } }
  public static class TC230 { public static final int FOO; static { FOO = 231; } }
  public static class TC231 { public static final int FOO; static { FOO = 232; } }
  public static class TC232 { public static final int FOO; static { FOO = 233; } }
  public static class TC233 { public static final int FOO; static { FOO = 234; } }
  public static class TC234 { public static final int FOO; static { FOO = 235; } }
  public static class TC235 { public static final int FOO; static { FOO = 236; } }
  public static class TC236 { public static final int FOO; static { FOO = 237; } }
  public static class TC237 { public static final int FOO; static { FOO = 238; } }
  public static class TC238 { public static final int FOO; static { FOO = 239; } }
  public static class TC239 { public static final int FOO; static { FOO = 240; } }
  public static class TC240 { public static final int FOO; static { FOO = 241; } }
  public static class TC241 { public static final int FOO; static { FOO = 242; } }
  public static class TC242 { public static final int FOO; static { FOO = 243; } }
  public static class TC243 { public static final int FOO; static { FOO = 244; } }
  public static class TC244 { public static final int FOO; static { FOO = 245; } }
  public static class TC245 { public static final int FOO; static { FOO = 246; } }
  public static class TC246 { public static final int FOO; static { FOO = 247; } }
  public static class TC247 { public static final int FOO; static { FOO = 248; } }
  public static class TC248 { public static final int FOO; static { FOO = 249; } }
  public static class TC249 { public static final int FOO; static { FOO = 250; } }

  public static int cnt = 0;

  public static void getNothing(int x) { System.out.println("In num " + x);}

  public static void doNothing() {
    int cur = cnt++;
    // These 'if' statements ensure that the interpreter won't just load all of the classes the
    // first time this function is run, before the JIT has a chance to do anything.
    if (cur == 0) {
      getNothing(TC0.FOO);
    } else if (cur == 1) {
      getNothing(TC1.FOO);
    } else if (cur == 2) {
      getNothing(TC2.FOO);
    } else if (cur == 3) {
      getNothing(TC3.FOO);
    } else if (cur == 4) {
      getNothing(TC4.FOO);
    } else if (cur == 5) {
      getNothing(TC5.FOO);
    } else if (cur == 6) {
      getNothing(TC6.FOO);
    } else if (cur == 7) {
      getNothing(TC7.FOO);
    } else if (cur == 8) {
      getNothing(TC8.FOO);
    } else if (cur == 9) {
      getNothing(TC9.FOO);
    } else if (cur == 10) {
      getNothing(TC10.FOO);
    } else if (cur == 11) {
      getNothing(TC11.FOO);
    } else if (cur == 12) {
      getNothing(TC12.FOO);
    } else if (cur == 13) {
      getNothing(TC13.FOO);
    } else if (cur == 14) {
      getNothing(TC14.FOO);
    } else if (cur == 15) {
      getNothing(TC15.FOO);
    } else if (cur == 16) {
      getNothing(TC16.FOO);
    } else if (cur == 17) {
      getNothing(TC17.FOO);
    } else if (cur == 18) {
      getNothing(TC18.FOO);
    } else if (cur == 19) {
      getNothing(TC19.FOO);
    } else if (cur == 20) {
      getNothing(TC20.FOO);
    } else if (cur == 21) {
      getNothing(TC21.FOO);
    } else if (cur == 22) {
      getNothing(TC22.FOO);
    } else if (cur == 23) {
      getNothing(TC23.FOO);
    } else if (cur == 24) {
      getNothing(TC24.FOO);
    } else if (cur == 25) {
      getNothing(TC25.FOO);
    } else if (cur == 26) {
      getNothing(TC26.FOO);
    } else if (cur == 27) {
      getNothing(TC27.FOO);
    } else if (cur == 28) {
      getNothing(TC28.FOO);
    } else if (cur == 29) {
      getNothing(TC29.FOO);
    } else if (cur == 30) {
      getNothing(TC30.FOO);
    } else if (cur == 31) {
      getNothing(TC31.FOO);
    } else if (cur == 32) {
      getNothing(TC32.FOO);
    } else if (cur == 33) {
      getNothing(TC33.FOO);
    } else if (cur == 34) {
      getNothing(TC34.FOO);
    } else if (cur == 35) {
      getNothing(TC35.FOO);
    } else if (cur == 36) {
      getNothing(TC36.FOO);
    } else if (cur == 37) {
      getNothing(TC37.FOO);
    } else if (cur == 38) {
      getNothing(TC38.FOO);
    } else if (cur == 39) {
      getNothing(TC39.FOO);
    } else if (cur == 40) {
      getNothing(TC40.FOO);
    } else if (cur == 41) {
      getNothing(TC41.FOO);
    } else if (cur == 42) {
      getNothing(TC42.FOO);
    } else if (cur == 43) {
      getNothing(TC43.FOO);
    } else if (cur == 44) {
      getNothing(TC44.FOO);
    } else if (cur == 45) {
      getNothing(TC45.FOO);
    } else if (cur == 46) {
      getNothing(TC46.FOO);
    } else if (cur == 47) {
      getNothing(TC47.FOO);
    } else if (cur == 48) {
      getNothing(TC48.FOO);
    } else if (cur == 49) {
      getNothing(TC49.FOO);
    } else if (cur == 50) {
      getNothing(TC50.FOO);
    } else if (cur == 51) {
      getNothing(TC51.FOO);
    } else if (cur == 52) {
      getNothing(TC52.FOO);
    } else if (cur == 53) {
      getNothing(TC53.FOO);
    } else if (cur == 54) {
      getNothing(TC54.FOO);
    } else if (cur == 55) {
      getNothing(TC55.FOO);
    } else if (cur == 56) {
      getNothing(TC56.FOO);
    } else if (cur == 57) {
      getNothing(TC57.FOO);
    } else if (cur == 58) {
      getNothing(TC58.FOO);
    } else if (cur == 59) {
      getNothing(TC59.FOO);
    } else if (cur == 60) {
      getNothing(TC60.FOO);
    } else if (cur == 61) {
      getNothing(TC61.FOO);
    } else if (cur == 62) {
      getNothing(TC62.FOO);
    } else if (cur == 63) {
      getNothing(TC63.FOO);
    } else if (cur == 64) {
      getNothing(TC64.FOO);
    } else if (cur == 65) {
      getNothing(TC65.FOO);
    } else if (cur == 66) {
      getNothing(TC66.FOO);
    } else if (cur == 67) {
      getNothing(TC67.FOO);
    } else if (cur == 68) {
      getNothing(TC68.FOO);
    } else if (cur == 69) {
      getNothing(TC69.FOO);
    } else if (cur == 70) {
      getNothing(TC70.FOO);
    } else if (cur == 71) {
      getNothing(TC71.FOO);
    } else if (cur == 72) {
      getNothing(TC72.FOO);
    } else if (cur == 73) {
      getNothing(TC73.FOO);
    } else if (cur == 74) {
      getNothing(TC74.FOO);
    } else if (cur == 75) {
      getNothing(TC75.FOO);
    } else if (cur == 76) {
      getNothing(TC76.FOO);
    } else if (cur == 77) {
      getNothing(TC77.FOO);
    } else if (cur == 78) {
      getNothing(TC78.FOO);
    } else if (cur == 79) {
      getNothing(TC79.FOO);
    } else if (cur == 80) {
      getNothing(TC80.FOO);
    } else if (cur == 81) {
      getNothing(TC81.FOO);
    } else if (cur == 82) {
      getNothing(TC82.FOO);
    } else if (cur == 83) {
      getNothing(TC83.FOO);
    } else if (cur == 84) {
      getNothing(TC84.FOO);
    } else if (cur == 85) {
      getNothing(TC85.FOO);
    } else if (cur == 86) {
      getNothing(TC86.FOO);
    } else if (cur == 87) {
      getNothing(TC87.FOO);
    } else if (cur == 88) {
      getNothing(TC88.FOO);
    } else if (cur == 89) {
      getNothing(TC89.FOO);
    } else if (cur == 90) {
      getNothing(TC90.FOO);
    } else if (cur == 91) {
      getNothing(TC91.FOO);
    } else if (cur == 92) {
      getNothing(TC92.FOO);
    } else if (cur == 93) {
      getNothing(TC93.FOO);
    } else if (cur == 94) {
      getNothing(TC94.FOO);
    } else if (cur == 95) {
      getNothing(TC95.FOO);
    } else if (cur == 96) {
      getNothing(TC96.FOO);
    } else if (cur == 97) {
      getNothing(TC97.FOO);
    } else if (cur == 98) {
      getNothing(TC98.FOO);
    } else if (cur == 99) {
      getNothing(TC99.FOO);
    } else if (cur == 100) {
      getNothing(TC100.FOO);
    } else if (cur == 101) {
      getNothing(TC101.FOO);
    } else if (cur == 102) {
      getNothing(TC102.FOO);
    } else if (cur == 103) {
      getNothing(TC103.FOO);
    } else if (cur == 104) {
      getNothing(TC104.FOO);
    } else if (cur == 105) {
      getNothing(TC105.FOO);
    } else if (cur == 106) {
      getNothing(TC106.FOO);
    } else if (cur == 107) {
      getNothing(TC107.FOO);
    } else if (cur == 108) {
      getNothing(TC108.FOO);
    } else if (cur == 109) {
      getNothing(TC109.FOO);
    } else if (cur == 110) {
      getNothing(TC110.FOO);
    } else if (cur == 111) {
      getNothing(TC111.FOO);
    } else if (cur == 112) {
      getNothing(TC112.FOO);
    } else if (cur == 113) {
      getNothing(TC113.FOO);
    } else if (cur == 114) {
      getNothing(TC114.FOO);
    } else if (cur == 115) {
      getNothing(TC115.FOO);
    } else if (cur == 116) {
      getNothing(TC116.FOO);
    } else if (cur == 117) {
      getNothing(TC117.FOO);
    } else if (cur == 118) {
      getNothing(TC118.FOO);
    } else if (cur == 119) {
      getNothing(TC119.FOO);
    } else if (cur == 120) {
      getNothing(TC120.FOO);
    } else if (cur == 121) {
      getNothing(TC121.FOO);
    } else if (cur == 122) {
      getNothing(TC122.FOO);
    } else if (cur == 123) {
      getNothing(TC123.FOO);
    } else if (cur == 124) {
      getNothing(TC124.FOO);
    } else if (cur == 125) {
      getNothing(TC125.FOO);
    } else if (cur == 126) {
      getNothing(TC126.FOO);
    } else if (cur == 127) {
      getNothing(TC127.FOO);
    } else if (cur == 128) {
      getNothing(TC128.FOO);
    } else if (cur == 129) {
      getNothing(TC129.FOO);
    } else if (cur == 130) {
      getNothing(TC130.FOO);
    } else if (cur == 131) {
      getNothing(TC131.FOO);
    } else if (cur == 132) {
      getNothing(TC132.FOO);
    } else if (cur == 133) {
      getNothing(TC133.FOO);
    } else if (cur == 134) {
      getNothing(TC134.FOO);
    } else if (cur == 135) {
      getNothing(TC135.FOO);
    } else if (cur == 136) {
      getNothing(TC136.FOO);
    } else if (cur == 137) {
      getNothing(TC137.FOO);
    } else if (cur == 138) {
      getNothing(TC138.FOO);
    } else if (cur == 139) {
      getNothing(TC139.FOO);
    } else if (cur == 140) {
      getNothing(TC140.FOO);
    } else if (cur == 141) {
      getNothing(TC141.FOO);
    } else if (cur == 142) {
      getNothing(TC142.FOO);
    } else if (cur == 143) {
      getNothing(TC143.FOO);
    } else if (cur == 144) {
      getNothing(TC144.FOO);
    } else if (cur == 145) {
      getNothing(TC145.FOO);
    } else if (cur == 146) {
      getNothing(TC146.FOO);
    } else if (cur == 147) {
      getNothing(TC147.FOO);
    } else if (cur == 148) {
      getNothing(TC148.FOO);
    } else if (cur == 149) {
      getNothing(TC149.FOO);
    } else if (cur == 150) {
      getNothing(TC150.FOO);
    } else if (cur == 151) {
      getNothing(TC151.FOO);
    } else if (cur == 152) {
      getNothing(TC152.FOO);
    } else if (cur == 153) {
      getNothing(TC153.FOO);
    } else if (cur == 154) {
      getNothing(TC154.FOO);
    } else if (cur == 155) {
      getNothing(TC155.FOO);
    } else if (cur == 156) {
      getNothing(TC156.FOO);
    } else if (cur == 157) {
      getNothing(TC157.FOO);
    } else if (cur == 158) {
      getNothing(TC158.FOO);
    } else if (cur == 159) {
      getNothing(TC159.FOO);
    } else if (cur == 160) {
      getNothing(TC160.FOO);
    } else if (cur == 161) {
      getNothing(TC161.FOO);
    } else if (cur == 162) {
      getNothing(TC162.FOO);
    } else if (cur == 163) {
      getNothing(TC163.FOO);
    } else if (cur == 164) {
      getNothing(TC164.FOO);
    } else if (cur == 165) {
      getNothing(TC165.FOO);
    } else if (cur == 166) {
      getNothing(TC166.FOO);
    } else if (cur == 167) {
      getNothing(TC167.FOO);
    } else if (cur == 168) {
      getNothing(TC168.FOO);
    } else if (cur == 169) {
      getNothing(TC169.FOO);
    } else if (cur == 170) {
      getNothing(TC170.FOO);
    } else if (cur == 171) {
      getNothing(TC171.FOO);
    } else if (cur == 172) {
      getNothing(TC172.FOO);
    } else if (cur == 173) {
      getNothing(TC173.FOO);
    } else if (cur == 174) {
      getNothing(TC174.FOO);
    } else if (cur == 175) {
      getNothing(TC175.FOO);
    } else if (cur == 176) {
      getNothing(TC176.FOO);
    } else if (cur == 177) {
      getNothing(TC177.FOO);
    } else if (cur == 178) {
      getNothing(TC178.FOO);
    } else if (cur == 179) {
      getNothing(TC179.FOO);
    } else if (cur == 180) {
      getNothing(TC180.FOO);
    } else if (cur == 181) {
      getNothing(TC181.FOO);
    } else if (cur == 182) {
      getNothing(TC182.FOO);
    } else if (cur == 183) {
      getNothing(TC183.FOO);
    } else if (cur == 184) {
      getNothing(TC184.FOO);
    } else if (cur == 185) {
      getNothing(TC185.FOO);
    } else if (cur == 186) {
      getNothing(TC186.FOO);
    } else if (cur == 187) {
      getNothing(TC187.FOO);
    } else if (cur == 188) {
      getNothing(TC188.FOO);
    } else if (cur == 189) {
      getNothing(TC189.FOO);
    } else if (cur == 190) {
      getNothing(TC190.FOO);
    } else if (cur == 191) {
      getNothing(TC191.FOO);
    } else if (cur == 192) {
      getNothing(TC192.FOO);
    } else if (cur == 193) {
      getNothing(TC193.FOO);
    } else if (cur == 194) {
      getNothing(TC194.FOO);
    } else if (cur == 195) {
      getNothing(TC195.FOO);
    } else if (cur == 196) {
      getNothing(TC196.FOO);
    } else if (cur == 197) {
      getNothing(TC197.FOO);
    } else if (cur == 198) {
      getNothing(TC198.FOO);
    } else if (cur == 199) {
      getNothing(TC199.FOO);
    } else if (cur == 200) {
      getNothing(TC200.FOO);
    } else if (cur == 201) {
      getNothing(TC201.FOO);
    } else if (cur == 202) {
      getNothing(TC202.FOO);
    } else if (cur == 203) {
      getNothing(TC203.FOO);
    } else if (cur == 204) {
      getNothing(TC204.FOO);
    } else if (cur == 205) {
      getNothing(TC205.FOO);
    } else if (cur == 206) {
      getNothing(TC206.FOO);
    } else if (cur == 207) {
      getNothing(TC207.FOO);
    } else if (cur == 208) {
      getNothing(TC208.FOO);
    } else if (cur == 209) {
      getNothing(TC209.FOO);
    } else if (cur == 210) {
      getNothing(TC210.FOO);
    } else if (cur == 211) {
      getNothing(TC211.FOO);
    } else if (cur == 212) {
      getNothing(TC212.FOO);
    } else if (cur == 213) {
      getNothing(TC213.FOO);
    } else if (cur == 214) {
      getNothing(TC214.FOO);
    } else if (cur == 215) {
      getNothing(TC215.FOO);
    } else if (cur == 216) {
      getNothing(TC216.FOO);
    } else if (cur == 217) {
      getNothing(TC217.FOO);
    } else if (cur == 218) {
      getNothing(TC218.FOO);
    } else if (cur == 219) {
      getNothing(TC219.FOO);
    } else if (cur == 220) {
      getNothing(TC220.FOO);
    } else if (cur == 221) {
      getNothing(TC221.FOO);
    } else if (cur == 222) {
      getNothing(TC222.FOO);
    } else if (cur == 223) {
      getNothing(TC223.FOO);
    } else if (cur == 224) {
      getNothing(TC224.FOO);
    } else if (cur == 225) {
      getNothing(TC225.FOO);
    } else if (cur == 226) {
      getNothing(TC226.FOO);
    } else if (cur == 227) {
      getNothing(TC227.FOO);
    } else if (cur == 228) {
      getNothing(TC228.FOO);
    } else if (cur == 229) {
      getNothing(TC229.FOO);
    } else if (cur == 230) {
      getNothing(TC230.FOO);
    } else if (cur == 231) {
      getNothing(TC231.FOO);
    } else if (cur == 232) {
      getNothing(TC232.FOO);
    } else if (cur == 233) {
      getNothing(TC233.FOO);
    } else if (cur == 234) {
      getNothing(TC234.FOO);
    } else if (cur == 235) {
      getNothing(TC235.FOO);
    } else if (cur == 236) {
      getNothing(TC236.FOO);
    } else if (cur == 237) {
      getNothing(TC237.FOO);
    } else if (cur == 238) {
      getNothing(TC238.FOO);
    } else if (cur == 239) {
      getNothing(TC239.FOO);
    } else if (cur == 240) {
      getNothing(TC240.FOO);
    } else if (cur == 241) {
      getNothing(TC241.FOO);
    } else if (cur == 242) {
      getNothing(TC242.FOO);
    } else if (cur == 243) {
      getNothing(TC243.FOO);
    } else if (cur == 244) {
      getNothing(TC244.FOO);
    } else if (cur == 245) {
      getNothing(TC245.FOO);
    } else if (cur == 246) {
      getNothing(TC246.FOO);
    } else if (cur == 247) {
      getNothing(TC247.FOO);
    } else if (cur == 248) {
      getNothing(TC248.FOO);
    } else if (cur == 249) {
      getNothing(TC249.FOO);
    }
  }

  public static void main(String[] args) throws Exception {
    Thread wait_thread = new Thread("Wait thread");
    Thread redefine_thread = new Thread("Redefine Thread");
    startWaitThread(wait_thread);
    startRedefineThread(redefine_thread);
    // Force initialize.
    TargetClass.class.toString();
    while (shouldContinue()) {
      doNothing();
    }
    wait_thread.join();
    redefine_thread.join();
  }

  public static native boolean shouldContinue();
  public static native void startWaitThread(Thread thr);
  public static native void startRedefineThread(Thread thr);
}
