// 蓝桥杯 - 三个英雄攻击敌人
// 敌人初始血量 2025
// 英雄1: 每回合攻击 5
// 英雄2: 奇数回合 15，偶数回合 2
// 英雄3: 回合%3==1 → 2, %3==2 → 10, %3==0 → 7

main {
    hp = 2025;
    round = 0;

    while (hp > 0) {
        round = round + 1;

        // 英雄1: 固定 5
        damage = 5;

        // 英雄2: 奇偶判断
        if (round % 2 == 1) {
            damage = damage + 15;
        } else {
            damage = damage + 2;
        }

        // 英雄3: 除以3的余数判断
        rem = round % 3;
        if (rem == 1) {
            damage = damage + 2;
        } else {
            if (rem == 2) {
                damage = damage + 10;
            } else {
                damage = damage + 7;
            }
        }

        hp = hp - damage;
    }

    see("result: ");
    see(round);
    see("\n");
}
