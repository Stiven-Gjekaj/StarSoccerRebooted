#include "ssr_chance.h"

#include <math.h>
#include <string.h>

#define D2R 0.017453292f
#define R2D 57.29578f

static float distance(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return (float)sqrt((double)(dx * dx + dy * dy));
}

static float direction_to(float ax, float ay, float bx, float by) {
    float a = (float)(atan2((double)(by - ay), (double)(bx - ax)) * (double)R2D);
    return a < 0 ? a + 360 : a;
}

static float cos_degrees(float a) {
    return (float)cos((double)(a * D2R));
}

static float sin_degrees(float a) {
    return (float)sin((double)(a * D2R));
}

// The x of the start of the ball of a midfield or attack chance.
static float start_x(int pitchSide, SSRRandom *random) {
    if (pitchSide == 0) return ssr_rand_float(random, 140, 320);
    if (pitchSide == 2) return ssr_rand_float(random, 320, 500);
    return ssr_rand_float(random, 140, 500);
}

void ssr_chance_ball_start(int type, int pitchSide, int minute, SSRRandom *random, float *x, float *y, float *z) {
    int gl = SSR_GOAL_LINE;
    *z = 0;
    switch (type) {
    case 1:
    case 9:
        *x = start_x(pitchSide, random);
        *y = ssr_rand_float(random, 550, 800);
        if (type == 9) *z = 57.6f;
        break;
    case 2:
        *x = start_x(pitchSide, random);
        *y = (float)(gl + 15 * 12 + 15 * ssr_rand(random, 30));
        break;
    case 10:
        if (pitchSide == 1) {
            *x = ssr_rand_float(random, 180, 460);
            *y = (float)(gl + 90 + 15 * ssr_rand(random, 10));
        } else {
            *x = pitchSide == 0 ? ssr_rand_float(random, 140, 320) : ssr_rand_float(random, 320, 500);
            *y = (float)(gl + 150 + 15 * ssr_rand(random, 20));
        }
        *z = 57.6f;
        break;
    case 3:
        *x = ssr_rand_float(random, 80, 540);
        *y = (float)(gl + 15 * 19 + 15 * ssr_rand(random, 25));
        break;
    case 4:
        *x = 320;
        *y = (float)(gl + 180);
        break;
    case 7:
        *x = minute % 2 == 0 ? 538 : 102;
        *y = 830;
        break;
    case 8: {
        float v = ssr_rand_float(random, 140, 500);
        *x = minute % 2 == 0 ? v : 640 - v;
        *y = ssr_rand_float(random, 620, 820);
        break;
    }
    default:
        *x = 320;
        *y = 600;
        break;
    }
}

// The team-mates of a chance that Vision brings: one for each draw that
// Vision beats.
static void extras(SSRChance *c, int draws, int range, int near, SSRRandom *random) {
    for (int i = 0; i < draws; i++) {
        if (c->setup.vision > ssr_rand(random, range)) {
            ssr_players_add_attacker(&c->players, &c->info, c->setup.teamStrength, near, random);
        }
    }
}

static void defenders(SSRChance *c, int count, int inWall, int high, SSRRandom *random) {
    for (int i = 0; i < count; i++) {
        ssr_players_add_defender(&c->players, &c->info, c->setup.opponentStrength, inWall, high, 0, random);
    }
}

static void attackers(SSRChance *c, int count, int near, SSRRandom *random) {
    for (int i = 0; i < count; i++) ssr_players_add_attacker(&c->players, &c->info, c->setup.teamStrength, near, random);
}

static void keeper(SSRChance *c, SSRRandom *random) {
    ssr_players_add_keeper(&c->players, &c->info, c->setup.opponentStrength, random);
}

// m_SetUpDribbling: four defenders in a staggered line or in a ring, the
// ball at (320, 850), and the finish line at y = 200.
// The factors of the match ball go to the ball and to the tap ball, which
// takes the weight and the bounce only.
static void use_match_ball(SSRChance *c) {
    const SSRChanceSetup *s = &c->setup;
    c->ball.speedFactor = s->ballSpeed > 0 ? s->ballSpeed : 1;
    c->ball.bounceFactor = s->ballBounce > 0 ? s->ballBounce : 1;
    c->ball.weightFactor = s->ballWeight > 0 ? s->ballWeight : 1;
    c->ball.curlFactor = s->ballCurl > 0 ? s->ballCurl : 1;
    c->tap.weight = c->ball.weightFactor;
    c->tap.bounce = c->ball.bounceFactor;
}

static void start_dribble(SSRChance *c, SSRRandom *random) {
    c->info.startX = 320;
    c->info.startY = 850;
    SSRMatchPlayer *d[4];
    for (int i = 0; i < 4; i++) {
        d[i] = ssr_players_add_defender(&c->players, &c->info, c->setup.opponentStrength, 0, 1, 0, random);
    }
    if (!d[0] || !d[1] || !d[2] || !d[3]) return;
    if (ssr_rand(random, 2) == 1) {
        d[0]->x = 320 + ssr_rand_float(random, -100, 100);
        d[0]->y = 700;
        static const int LOW[3] = {10, 75, 150}, HIGH[3] = {150, 225, 350};
        for (int i = 1; i < 4; i++) {
            int left = d[i - 1]->x > 320;
            int r = ssr_rand2(random, LOW[i - 1], HIGH[i - 1]);
            d[i]->x = (float)(left ? 320 - r : 320 + r);
            d[i]->y = (float)(850 - 150 * (i + 1));
        }
    } else {
        float a0 = ssr_rand_float(random, 0, 360), r = ssr_rand_float(random, 150, 250);
        for (int i = 0; i < 4; i++) {
            float a = a0 + 90.0f * (float)i;
            // The fourth uses 290 for its y, as the game does.
            float ay = i == 3 ? a0 + 290 : a;
            d[i]->x = 320 + (float)cos((double)(a * 0.017453292f)) * r;
            d[i]->y = 500 + (float)sin((double)(ay * 0.017453292f)) * r;
            d[i]->x += (float)ssr_rand2(random, -75, 75);
            d[i]->y += (float)ssr_rand2(random, -25, 25);
        }
    }
    // c_TBall.m_Reset(5) resets the tap ball too, with its random numbers.
    SSRTapSetup tap = {.chanceType = 5, .level = c->setup.level, .careerApps = c->setup.careerApps,
                       .careerGoals = c->setup.careerGoals, .careerPasses = c->setup.careerPasses};
    ssr_tapball_reset(&c->tap, &tap, random);
    ssr_ball_place(&c->ball, 320, 850);
    use_match_ball(c);
    c->kicking = 3;
    c->actionType = 11;
    c->state = SSR_CHANCE_DRIBBLE;
}

static void start_defend(SSRChance *c, SSRRandom *random);

void ssr_chance_start_training(SSRChance *c, const SSRChanceSetup *setup, int training, float x, float y,
                               SSRRandom *random) {
    memset(c, 0, sizeof(*c));
    c->setup = *setup;
    c->info.training = training;
    c->info.starLevel = setup->starLevel;
    c->info.clubId = setup->clubId;
    c->info.pitchSide = 1;
    c->goal = ssr_goal(1);
    ssr_ball_place(&c->ball, x, y);
    c->ball.windSpeed = setup->windSpeed;
    c->ball.windDirection = setup->windDirection;
    c->info.startX = x;
    c->info.startY = y;
    c->actionType = 5;
    // c_TBall.m_Reset(5) resets the tap ball, with its random numbers.
    SSRTapSetup tap = {.training = training, .chanceType = setup->type, .level = setup->level};
    ssr_tapball_reset(&c->tap, &tap, random);
    ssr_players_clear(&c->players);
    if (training == 8) {
        c->state = SSR_CHANCE_DRIBBLE;
        c->kicking = 3;
    }
}

void ssr_chance_start(SSRChance *c, const SSRChanceSetup *setup, SSRRandom *random) {
    memset(c, 0, sizeof(*c));
    c->setup = *setup;
    c->info.chanceType = setup->type;
    c->info.minute = setup->minute;
    c->info.starLevel = setup->starLevel;
    c->info.clubId = setup->clubId;
    c->info.captain = setup->captain;
    // LoadPitchImages chooses the side of the pitch. A penalty has its own
    // pitch, with the goal in the middle [I].
    c->info.pitchSide = ssr_rand(random, 3) - 1;
    if (setup->type == 4) c->info.pitchSide = 1;
    c->goal = ssr_goal(c->info.pitchSide);
    float x, y, z;
    ssr_chance_ball_start(setup->type, c->info.pitchSide, setup->minute, random, &x, &y, &z);
    ssr_ball_place(&c->ball, (float)(int)x, (float)(int)y);
    c->ball.z = c->ball.oldZ = z;
    c->ball.windSpeed = setup->windSpeed;
    c->ball.windDirection = setup->windDirection;
    c->ball.weather = setup->weather;
    c->info.startX = (float)(int)x;
    c->info.startY = (float)(int)y;
    c->actionType = setup->type;
    SSRTapSetup tap = {.chanceType = setup->type,
                       .header = setup->type == 9 || setup->type == 10,
                       .careerApps = setup->careerApps,
                       .chances = setup->chances,
                       .careerGoals = setup->careerGoals,
                       .careerPasses = setup->careerPasses,
                       .level = setup->level,
                       .opponentStrength = setup->opponentStrength};
    ssr_tapball_reset(&c->tap, &tap, random);
    use_match_ball(c);

    ssr_players_clear(&c->players);
    if (setup->type == 11) {
        start_dribble(c, random);
        return;
    }
    if (setup->type == SSR_CHANCE_SAVE || setup->type == SSR_CHANCE_BLOCK) {
        start_defend(c, random);
        return;
    }
    switch (setup->type) {
    case 1:
        defenders(c, 8, 0, 1, random);
        attackers(c, 2, 1, random);
        extras(c, 1, 60, 1, random);
        break;
    case 2:
    case 10:
        keeper(c, random);
        defenders(c, 8, 0, 0, random);
        attackers(c, 1, 1, random);
        extras(c, 2, 60, 1, random);
        break;
    case 9:
        defenders(c, 8, 0, 1, random);
        attackers(c, 2, 1, random);
        extras(c, 1, 60, 1, random);
        break;
    case 3:
        keeper(c, random);
        defenders(c, 3, 0, 0, random);
        defenders(c, 5, 1, 0, random);
        extras(c, 2, 70, 0, random);
        break;
    case 7:
    case 8:
        keeper(c, random);
        attackers(c, 2, 0, random);
        extras(c, 1, 70, 0, random);
        defenders(c, 4, 0, 0, random);
        if (setup->type == 8) {
            ssr_players_add_defender(&c->players, &c->info, setup->opponentStrength, 0, 0, 1, random);
        }
        break;
    case 4:
        keeper(c, random);
        defenders(c, 8, 0, 0, random);
        attackers(c, 4, 0, random);
        break;
    default:
        break;
    }
    ssr_players_place_wall(&c->players, &c->info, c->ball.x, c->ball.y);
    ssr_players_remove_off_screen(&c->players, 0, 640);
    c->state = SSR_CHANCE_AIM;
}

SSRPlayerBall ssr_chance_player_ball(const SSRChance *c) {
    SSRPlayerBall b = {c->ball.x,       c->ball.y,     c->ball.z,         c->ball.power,
                       c->ball.direction, c->ball.alive, c->kicking,        c->lastTouchedBy,
                       c->kickTime,     c->actionType, c->bounceX,       c->bounceY};
    return b;
}

static SSRMatchPlayer *find_player(SSRChance *c, int id) {
    for (int i = 0; i < c->players.count; i++) {
        if (c->players.list[i].id == id) return &c->players.list[i];
    }
    return NULL;
}

static void finish(SSRChance *c, SSRChanceResult result) {
    if (c->result == SSR_RESULT_NONE) c->result = result;
    c->ball.alive = 0;
    c->state = SSR_CHANCE_OVER;
}

// CheckOffsideAll: the team-mates past the line when the ball is kicked.
static void check_offside(SSRChance *c) {
    SSRPlayerBall b = ssr_chance_player_ball(c);
    int line = ssr_players_offside_line(&c->players, &c->info, &b);
    for (int i = 0; i < c->players.count; i++) {
        const SSRMatchPlayer *p = &c->players.list[i];
        if (p->type != SSR_PLAYER_ATTACKER || p->id == c->lastTouchedBy || p->id > SSR_PLAYERS_MAX) continue;
        c->offside[p->id] = (unsigned char)ssr_player_offside(p, &c->info, line);
    }
}

static float clamp_volume(float volume, float low) {
    return volume < low ? low : volume > 1 ? 1 : volume;
}

// c_AudioManager.Play: the app plays the sound after the update.
static void sound(SSRChance *c, const char *name, float volume) {
    if (c->soundCount >= SSR_CHANCE_SOUNDS) return;
    c->sounds[c->soundCount] = name;
    c->soundVolumes[c->soundCount++] = volume;
}

// c_TBall.m_Update: moves the ball, and plays the sounds of its bounce, of
// a post, of the net, and of a goal.
static void move_ball(SSRChance *c, const SSRGoal *goal, SSRRandom *random) {
    SSRBall *b = &c->ball;
    int bounces = b->bounces, postHit = b->postHit, inGoal = b->inGoal, netHit = b->netHit;
    ssr_ball_update(b, goal, random);
    float volume = clamp_volume(b->power * 0.2f, 0.1f);
    if (b->bounces > bounces && b->zVelocity > 0.5f) sound(c, "BallBounce", volume);
    if (b->postHit && !postHit) sound(c, "BallColGoalPost", volume);
    if (b->netHit && !netHit) sound(c, "Sounds/HitNet", volume);
    if (b->inGoal && !inGoal && c->info.training == 0) sound(c, "Sounds/CrowdGoal", 1);
}

static void kick(SSRChance *c, int by, float direction, float power, float dip, float curl, float height, int header) {
    ssr_ball_kick(&c->ball, direction, power, dip, curl, height, header);
    sound(c, power > 65 ? "BallKickLong" : "BallKickShort", clamp_volume(c->ball.power, 0.35f));
    c->kicking = 2;
    c->kickTime = c->now;
    c->lastTouchedBy = by;
    c->kicked = 1;
    check_offside(c);
}

// c_TBall.Deflect off a player that does not keep the ball [I]: it turns
// and slows down.
static void deflect(SSRChance *c, const SSRMatchPlayer *p, SSRRandom *random) {
    c->deflects++;
    c->ball.direction = direction_to(p->x, p->y, c->ball.x, c->ball.y) + ssr_rand_float(random, -30, 30);
    c->ball.power *= 0.5f;
    c->ball.curl = 0;
    c->lastTouchedBy = p->id;
    c->kickTime = c->now;
}

// A team-mate gets the ball in an attack and shoots at the goal.
static void mate_shoots(SSRChance *c, const SSRMatchPlayer *p, SSRRandom *random) {
    float dist = distance(c->ball.x, c->ball.y, 320, (float)SSR_GOAL_LINE);
    float g = ssr_goal_centre(c->info.pitchSide), line = (float)SSR_GOAL_LINE;
    if (c->info.chanceType == 7 || c->info.chanceType == 8) {
        g = c->info.minute % 2 == 0 ? 540 : 100;
        line = 152;
    }
    float sp = c->ball.power > 4.5f ? 2.5f : 1.25f;
    static const float TARGETS[] = {-52.5f, -30, 0, 30, 52.5f};
    int r = ssr_rand(random, 5);
    float spread = r == 3 ? 2 * sp : sp;
    float direction = direction_to(p->x, p->y, g + TARGETS[r - 1], line) + ssr_rand_float(random, -spread, spread);
    if (dist < 180) {
        if (c->ball.x < g - 52.5f) {
            float t = ssr_rand(random, 2) == 1 ? -52.5f : -30;
            direction = direction_to(p->x, p->y, g + t, line) + ssr_rand_float(random, -sp, sp);
        } else if (c->ball.x > g + 52.5f) {
            float t = ssr_rand(random, 2) == 1 ? 30 : 52.5f;
            direction = direction_to(p->x, p->y, g + t, line) + ssr_rand_float(random, -sp, sp);
        }
    } else {
        // Aim at the open side of a keeper that is off the centre.
        for (int i = 0; i < c->players.count; i++) {
            const SSRMatchPlayer *k = &c->players.list[i];
            if (k->type != SSR_PLAYER_KEEPER || fabsf(g - k->x) < 15) continue;
            float t;
            if (k->x < g) {
                t = ssr_rand(random, 2) == 1 ? 30 : 52.5f;
            } else {
                t = ssr_rand(random, 2) == 1 ? -52.5f : -30;
            }
            direction = direction_to(p->x, p->y, g + t, line) + ssr_rand_float(random, -sp, sp);
        }
    }
    float high = c->ball.z > 24 ? 20 : 0;
    float curl = fabsf(ssr_rand_float(random, -0.25f, 0.25f));
    if (p->x > g) curl = -curl;
    float power = ssr_rand_float(random, 75, 90) - high;
    if (dist < 180) {
        kick(c, p->id, direction, power, 7.5e-4f * ssr_rand_float(random, 0, 75), 0, 0.1f * c->ball.z, 0);
    } else if (dist > 420) {
        kick(c, p->id, direction, power, 7.5e-4f * ssr_rand_float(random, 0, 90), curl, 0.2f * c->ball.z, 0);
    } else {
        kick(c, p->id, direction, power, 0, curl, 0.2f * c->ball.z, 0);
    }
}

static int is_pass_chance(int type) {
    return type == 1 || type == 9;
}

// c_TBall.m_CheckDefenders: the keeper, the defenders, and the team-mates
// against the ball.
static void check_players(SSRChance *c, SSRRandom *random) {
    SSRBall *b = &c->ball;
    if (c->lastTouchedBy > 0 && c->now < c->kickTime + 150) return;
    for (int i = 0; i < c->players.count && b->alive; i++) {
        SSRMatchPlayer *p = &c->players.list[i];
        float dx = fabsf(b->x - p->x), dy = fabsf(b->y - p->y);
        if (p->type == SSR_PLAYER_KEEPER) {
            if (p->id == c->lastTouchedBy) continue;
            int frame = ssr_player_frame(p);
            float s = p->strength, top = 48, wide = 15;
            if (frame == 12) {
                top = 60;
            } else if (frame == 15) {
                top = 0.1f * s + 40;
            } else if (frame == 16) {
                top = 0.1f * s + 42.5f;
                wide = 26;
            } else if (frame == 17) {
                top = 0.25f * s + 22;
                wide = 35;
            }
            if (!(b->z < top && dx < wide && dy < 8)) continue;
            if (c->lastTouchedBy == 0) c->shotsOnTarget++;
            c->lastTouchedBy = p->id;
            c->kickTime = c->now;
            if (frame < 15) {
                // The keeper catches the ball.
                b->power = 0;
                finish(c, SSR_RESULT_SAVED);
            } else {
                // The keeper pushes the ball away. It can still go in.
                sound(c, "BallParry", clamp_volume(b->power * 0.2f, 0.1f));
                b->direction = b->x > p->x ? ssr_rand_float(random, 5, 45) : ssr_rand_float(random, 135, 175);
                b->power *= 0.3f;
                b->y = p->y + 4;
                b->windForce = b->curl = b->dip = 0;
                c->result = SSR_RESULT_SAVED;
            }
            return;
        }
        if (distance(b->x, b->y, p->x, p->y) >= 15 || b->z >= 48 || p->id == c->lastTouchedBy) continue;
        if (p->type == SSR_PLAYER_DEFENDER) {
            if (b->power > 7.5f && b->z > 24 && b->z < 43.2f && c->deflects < 2) {
                deflect(c, p, random);
                return;
            }
            // The defender controls the ball, then clears it in an attack.
            static const float FX[] = {8, 0, -8, 0}, FY[] = {-2, 8, -2, -8};
            b->x = p->x + FX[p->facing & 3];
            b->y = p->y + FY[p->facing & 3];
            b->power = 0;
            if (is_pass_chance(c->info.chanceType) || c->info.chanceType == 6) {
                finish(c, SSR_RESULT_INTERCEPTED);
                return;
            }
            float dir = direction_to(p->x, p->y, p->x, 960) + ssr_rand_float(random, -7.5f, 7.5f);
            float power = ssr_rand_float(random, 55, 80);
            float dip = ssr_rand_float(random, 90, 100) * 7.5e-4f;
            float curl = ssr_rand_float(random, -0.25f, 0.25f);
            kick(c, p->id, dir, power, dip, curl, 0.2f * b->z, 0);
            c->result = SSR_RESULT_INTERCEPTED;
            if (b->kicks > 1) finish(c, SSR_RESULT_INTERCEPTED);
            return;
        }
        // A team-mate.
        c->kickYards = distance(c->info.startX, c->info.startY, b->x, b->y) / 15;
        if (p->id <= SSR_PLAYERS_MAX && c->offside[p->id]) {
            b->power = 0;
            finish(c, SSR_RESULT_OFFSIDE);
            return;
        }
        if (is_pass_chance(c->info.chanceType)) {
            if (b->power > 7.5f && b->z > 24) {
                deflect(c, p, random);
                return;
            }
            b->power = 0;
            finish(c, !b->postHit && c->lastTouchedBy == 0 ? SSR_RESULT_PASS : SSR_RESULT_MISSED);
            return;
        }
        if (b->power > 7.5f && b->z > 24 && b->z < 43.2f) {
            deflect(c, p, random);
            return;
        }
        mate_shoots(c, p, random);
        return;
    }
}

// Where the ball comes down: a copy of the ball flies until it bounces.
static void predict_bounce(SSRChance *c) {
    c->bounceX = c->bounceY = -100;
    if (c->ball.inGoal || c->ball.z <= 15) return;
    SSRBall copy = c->ball;
    copy.holdStop = 1;
    int bounces = copy.bounces;
    for (int i = 0; i < 600 && copy.bounces == bounces; i++) ssr_ball_update(&copy, NULL, NULL);
    c->bounceX = copy.x;
    c->bounceY = copy.y;
}

static int players_moving(const SSRChance *c) {
    if (c->info.chanceType == 4) return 0;
    for (int i = 0; i < c->players.count; i++) {
        if (c->players.list[i].xVelocity != 0 && c->players.list[i].yVelocity != 0) return 1;
    }
    return 0;
}

static void update_players(SSRChance *c, SSRRandom *random) {
    SSRPlayerBall b = ssr_chance_player_ball(c);
    ssr_players_update(&c->players, &c->info, &b, c->now, random);
}

// c_TMatchPlayer.UpdateMakeRun for each team-mate that can make a run (the
// fallback form). A touch within 48 of the team-mate takes it. The drag
// sets the run, from 25 to the max. A release within 25 of the team-mate
// ends the run.
static void make_runs(SSRChance *c, const SSRTapTouch *touch) {
    SSRPlayers *ps = &c->players;
    for (int i = 0; i < ps->count; i++) {
        SSRMatchPlayer *p = &ps->list[i];
        if (!p->canMakeRun) continue;
        if (ps->makeRun == 0 || ps->makeRun != p->id) {
            if (distance(touch->x, touch->y, p->x, p->y) < 48 && touch->hit) ps->makeRun = p->id;
        } else if (!touch->down) {
            ps->makeRun = 0;
            if (distance(touch->x, touch->y, p->x, p->y) < 25) {
                p->makeRunDirection = 0;
                p->makeRunDistance = 0;
            }
        } else {
            p->makeRunDirection = direction_to(p->x, p->y, touch->x, touch->y);
            float d = distance(p->x, p->y, touch->x, touch->y);
            float most = (float)SSR_MAKE_RUN_MAX;
            p->makeRunDistance = d < 25 ? 25 : d > most ? most : d;
            p->makeRunX = (int)(p->x + cos_degrees(p->makeRunDirection) * p->makeRunDistance);
            p->makeRunY = (int)(p->y + sin_degrees(p->makeRunDirection) * p->makeRunDistance);
        }
    }
}

static void aim(SSRChance *c, const SSRTapTouch *touch, SSRRandom *random) {
    SSRBall *b = &c->ball;
    if (c->kicking == 0) {
        // The ball waits while the finger draws a run.
        if (c->players.makeRun == 0 && touch->hit && distance(touch->x, touch->y, b->x, b->y) < b->z * 0.5f + 48) {
            c->kicking = 1;
            c->touchTime = c->now;
            c->touchX = touch->x;
            c->touchY = touch->y;
            return;
        }
        make_runs(c, touch);
        return;
    }
    if (touch->down) {
        ssr_ball_aim(b, touch->x, touch->y, c->setup.power, &c->aimDirection, &c->aimPower);
    } else if (distance(touch->x, touch->y, c->touchX, c->touchY) > 10 || c->now > c->touchTime + 1000) {
        c->state = SSR_CHANCE_TAP;
        return;
    } else {
        c->kicking = 0;
        c->aimPower = 0;
        return;
    }
    update_players(c, random);
    // CheckClosedDown: a defender gets to the ball while the player aims.
    for (int i = 0; i < c->players.count; i++) {
        const SSRMatchPlayer *p = &c->players.list[i];
        if (p->type == SSR_PLAYER_DEFENDER && distance(p->x, p->y, b->x, b->y) < 15 && c->now > c->kickTime + 150) {
            c->kicking = 2;
            b->power = 0.1f;
            finish(c, SSR_RESULT_LOST);
            return;
        }
    }
}

// One tick of a dribble: the swipe, the ball, the tackles, the line.
static void dribble(SSRChance *c, const SSRTapTouch *touch, SSRRandom *random) {
    SSRBall *b = &c->ball;
    if (!c->dribbling) {
        // Nothing moves until the first touch.
        if (!touch->down) return;
        c->dribbling = 1;
    }
    b->oldX = b->x;
    b->oldY = b->y;
    if (!c->swiping && touch->down) {
        c->swiping = 1;
        c->swipeX = touch->x;
        c->swipeY = touch->y;
    } else if (c->swiping && !touch->down) {
        // The swipe adds a push along its way (c_TBall.m_CheckForSwipe).
        c->swiping = 0;
        float dir = direction_to(c->swipeX, c->swipeY, touch->x, touch->y);
        float dist = distance(c->swipeX, c->swipeY, touch->x, touch->y);
        if (dist < 10) dist = 10;
        if (dist > 180) dist = 180;
        float push = (dist + (float)c->setup.pace) * 0.009f;
        b->direction = dir;
        c->dribbleX += (float)cos((double)(dir * D2R)) * push;
        c->dribbleY += (float)sin((double)(dir * D2R)) * push;
    }
    // c_TBall.m_Move.
    float vx = c->dribbleX, vy = c->dribbleY;
    b->x += vx;
    b->y += vy;
    c->dribbleX *= 0.985f;
    c->dribbleY *= 0.985f;
    b->power = (float)sqrt((double)(vx * vx + vy * vy));
    // The dribble drill has cones, not defenders. The drill decides its end.
    if (c->info.training == 8) return;
    // A defender within 15 takes the ball.
    for (int i = 0; i < c->players.count; i++) {
        const SSRMatchPlayer *p = &c->players.list[i];
        if (p->type != SSR_PLAYER_DEFENDER || distance(b->x, b->y, p->x, p->y) >= 15) continue;
        static const float FX[] = {8, 0, -8, 0}, FY[] = {-2, 8, -2, -8};
        b->x = p->x + FX[p->facing & 3];
        b->y = p->y + FY[p->facing & 3];
        b->power = 0;
        c->lastTouchedBy = p->id;
        finish(c, SSR_RESULT_INTERCEPTED);
        break;
    }
    if (b->alive && (b->x < 0 || b->x > 640 || b->y < 0 || b->y > 960)) finish(c, SSR_RESULT_MISSED);
    update_players(c, random);
    // The finish line from (-200, 200) to (800, 200).
    if (b->alive && ((b->oldY >= 200 && b->y <= 200) || (b->oldY <= 200 && b->y >= 200)) && b->oldY != b->y) {
        float t = (200 - b->oldY) / (b->y - b->oldY);
        float x = b->oldX + (b->x - b->oldX) * t;
        if (x >= -200 && x <= 800) {
            c->kickYards = distance(320, 850, b->x, b->y) / 15;
            finish(c, SSR_RESULT_DRIBBLED);
        }
    }
}

/* ------------------------------------------------- moments in defence */

// The time before the other team kicks, in ms.
#define DEFEND_WAIT 900
// The distance at which the player stops the ball, and the height under
// which the player reaches it.
#define DEFEND_REACH 18.0f
#define DEFEND_HEIGHT 50.0f

static SSRMatchPlayer *place(SSRMatchPlayer *p, float x, float y, int facing) {
    if (!p) return NULL;
    p->x = x;
    p->y = y;
    p->xVelocity = p->yVelocity = 0;
    p->inWall = 0;
    p->facing = facing;
    p->direction = facing == 3 ? 270 : 90;
    p->anim = SSR_ANIM_STAND;
    return p;
}

// A save: a player of the other team with the ball, and the keeper of the
// player on the line. A block: a passer, a forward near the goal, the
// defender of the player between them, and a keeper of the team.
static void start_defend(SSRChance *c, SSRRandom *random) {
    float gl = (float)SSR_GOAL_LINE, centre = ssr_goal_centre(c->info.pitchSide);
    float strength = c->setup.opponentStrength;
    SSRMatchPlayer *p;
    if (c->setup.type == SSR_CHANCE_SAVE) {
        float x = centre + ssr_rand_float(random, -170, 170), y = ssr_rand_float(random, 380, 560);
        p = place(ssr_players_add_defender(&c->players, &c->info, strength, 0, 0, 0, random), x, y, 3);
        c->shooterId = p ? p->id : 0;
        p = place(ssr_players_add_keeper(&c->players, &c->info, c->setup.teamStrength, random), centre, gl + 14, 1);
        c->controlId = p ? p->id : 0;
        ssr_ball_place(&c->ball, x, y - 12);
    } else {
        float x = centre + ssr_rand_float(random, -200, 200), y = ssr_rand_float(random, 620, 760);
        p = place(ssr_players_add_defender(&c->players, &c->info, strength, 0, 0, 0, random), x, y, 3);
        c->shooterId = p ? p->id : 0;
        float fx = centre + ssr_rand_float(random, -120, 120), fy = ssr_rand_float(random, 260, 340);
        p = place(ssr_players_add_defender(&c->players, &c->info, strength, 0, 0, 0, random), fx, fy, 3);
        c->receiverId = p ? p->id : 0;
        float dx = (x + fx) / 2 + ssr_rand_float(random, -140, 140), dy = (y + fy) / 2 + ssr_rand_float(random, -40, 60);
        p = place(ssr_players_add_attacker(&c->players, &c->info, c->setup.teamStrength, 0, random), dx, dy, 1);
        c->controlId = p ? p->id : 0;
        place(ssr_players_add_keeper(&c->players, &c->info, c->setup.teamStrength, random), centre, gl + 14, 1);
        ssr_ball_place(&c->ball, x, y - 12);
    }
    const SSRMatchPlayer *me = find_player(c, c->controlId);
    c->targetX = me ? me->x : centre;
    c->targetY = me ? me->y : gl;
    c->info.startX = c->ball.x;
    c->info.startY = c->ball.y;
    c->actionType = c->setup.type;
    c->defendStart = -1;
    c->state = SSR_CHANCE_DEFEND;
}

// A player of the other team kicks at the goal, a little off the middle.
static void shoot_at_goal(SSRChance *c, int by, SSRRandom *random) {
    float centre = ssr_goal_centre(c->info.pitchSide);
    float target = centre + ssr_rand_float(random, -50, 50);
    float direction = direction_to(c->ball.x, c->ball.y, target, (float)SSR_GOAL_LINE);
    float power = ssr_rand_float(random, 70, 86) + c->setup.opponentStrength / 25;
    kick(c, by, direction, power, 7.5e-4f * ssr_rand_float(random, 0, 40), ssr_rand_float(random, -0.2f, 0.2f), 0, 0);
}

static void defend(SSRChance *c, const SSRTapTouch *touch, SSRRandom *random) {
    SSRBall *b = &c->ball;
    if (c->defendStart < 0) c->defendStart = c->now;
    SSRMatchPlayer *me = find_player(c, c->controlId);
    if (!me) {
        finish(c, SSR_RESULT_CONCEDED);
        return;
    }
    // Each tap moves the target. A keeper stays near the line.
    if (touch->down) {
        c->targetX = touch->x;
        c->targetY = touch->y;
        if (c->setup.type == SSR_CHANCE_SAVE) {
            float gl = (float)SSR_GOAL_LINE;
            c->targetY = c->targetY < gl ? gl : c->targetY > gl + 70 ? gl + 70 : c->targetY;
        }
    }
    float dx = c->targetX - me->x, dy = c->targetY - me->y, d = (float)sqrt((double)(dx * dx + dy * dy));
    // The speed comes from the pace of the player.
    float speed = 2.4f + (float)c->setup.pace / 50.0f;
    if (d > 1) {
        float step = d < speed ? d : speed;
        me->x += dx / d * step;
        me->y += dy / d * step;
        me->direction = direction_to(0, 0, dx, dy);
        me->anim = SSR_ANIM_RUN;
    } else {
        me->anim = SSR_ANIM_STAND;
    }
    // The other team kicks after the wait: a shot, or a pass to the forward.
    if (!c->kicking) {
        if (c->now - c->defendStart < DEFEND_WAIT) return;
        if (c->setup.type == SSR_CHANCE_SAVE) {
            shoot_at_goal(c, c->shooterId, random);
        } else {
            const SSRMatchPlayer *f = find_player(c, c->receiverId);
            float fx = f ? f->x : 320, fy = f ? f->y : 300;
            float dist = distance(b->x, b->y, fx, fy);
            kick(c, c->shooterId, direction_to(b->x, b->y, fx, fy), 30 + dist / 9, 0, 0, 0, 0);
        }
        return;
    }
    move_ball(c, &c->goal, random);
    if (b->outcome == SSR_BALL_GOAL) {
        finish(c, SSR_RESULT_CONCEDED);
        return;
    }
    // The player stops the ball.
    if (distance(me->x, me->y, b->x, b->y) < DEFEND_REACH && b->z < DEFEND_HEIGHT) {
        b->power = 0;
        c->lastTouchedBy = me->id;
        finish(c, SSR_RESULT_DEFENDED);
        return;
    }
    if (c->setup.type == SSR_CHANCE_BLOCK) {
        // The forward takes the pass and shoots. The keeper of the team
        // stops a shot that comes near.
        const SSRMatchPlayer *f = find_player(c, c->receiverId);
        if (!c->received && f && distance(f->x, f->y, b->x, b->y) < 14) {
            c->received = 1;
            b->x = f->x;
            b->y = f->y - 10;
            shoot_at_goal(c, f->id, random);
        }
        for (int i = 0; c->received && i < c->players.count; i++) {
            const SSRMatchPlayer *k = &c->players.list[i];
            if (k->type == SSR_PLAYER_KEEPER && distance(k->x, k->y, b->x, b->y) < 22 && b->z < DEFEND_HEIGHT) {
                b->power = 0;
                finish(c, SSR_RESULT_MISSED);
                return;
            }
        }
    }
    if (b->x < 0 || b->x > 640 || b->y < 0 || b->y > 960 || !b->alive || b->outcome == SSR_BALL_MISSED ||
        (b->power < 0.3f && c->now - c->kickTime > 500)) {
        finish(c, SSR_RESULT_MISSED);
    }
}

void ssr_chance_update(SSRChance *c, int now, const SSRTapTouch *touch, SSRRandom *random) {
    c->now = now;
    c->beep = c->kicked = 0;
    c->soundCount = 0;
    switch (c->state) {
    case SSR_CHANCE_AIM:
        aim(c, touch, random);
        break;
    case SSR_CHANCE_TAP: {
        SSRTapBallEvent event = ssr_tapball_update(&c->tap, now, touch, c->setup.controlBoots, &c->beep);
        if (c->beep) sound(c, "Sounds/StopWatchBeep", 1);
        if (event == SSR_TAPBALL_TAPPED) sound(c, "BallTap", 1);
        if (event == SSR_TAPBALL_KICK) {
            float direction, power, dip, curl, height;
            int header;
            ssr_tapball_kick(&c->tap, c->aimDirection, c->aimPower, c->setup.technique, &direction, &power, &dip, &curl,
                             &height, &header);
            kick(c, 0, direction, power, dip, curl, height, header);
            c->state = SSR_CHANCE_FLIGHT;
        } else if (event == SSR_TAPBALL_DUD) {
            kick(c, 0, 0, 1, 0, 0, 0, 0);
            finish(c, SSR_RESULT_MISSED);
        }
        break;
    }
    case SSR_CHANCE_FLIGHT: {
        SSRBall *b = &c->ball;
        const SSRMatchPlayer *toucher = c->lastTouchedBy > 0 ? find_player(c, c->lastTouchedBy) : NULL;
        // The wind stops after the keeper touches the ball.
        b->windSpeed = toucher && toucher->type == SSR_PLAYER_KEEPER ? 0 : c->setup.windSpeed;
        b->holdStop = players_moving(c);
            int training = c->info.training;
        int noGoal = is_pass_chance(c->info.chanceType) || c->info.chanceType == 12 || training == 2 || training == 6;
        int postHit = b->postHit;
        move_ball(c, noGoal ? NULL : &c->goal, random);
        if (!postHit && b->postHit && c->lastTouchedBy == 0) {
            c->shotsOnTarget++;
            c->postHits++;
        }
        predict_bounce(c);
        if (b->outcome == SSR_BALL_GOAL) {
            const SSRMatchPlayer *last = c->lastTouchedBy > 0 ? find_player(c, c->lastTouchedBy) : NULL;
            if (last && last->type == SSR_PLAYER_KEEPER) c->lastTouchedBy = 0;
            last = c->lastTouchedBy > 0 ? find_player(c, c->lastTouchedBy) : NULL;
            c->result = SSR_RESULT_NONE;
            finish(c, last && last->type == SSR_PLAYER_ATTACKER ? SSR_RESULT_ASSIST : SSR_RESULT_GOAL);
        } else if (b->alive) {
            if (training == 0 || training == 3 || training == 4) check_players(c, random);
            if (b->alive && (b->x < 0 || b->x > 640 || b->y < 0 || b->y > 960)) finish(c, SSR_RESULT_MISSED);
            // In training a ball that goes down the screen, or stops, dies.
            if (b->alive && training != 0 && training != 5) {
                float d = fmodf(b->direction, 360);
                if (d < 0) d += 360;
                if (d < 180 || b->power < 0.15f) finish(c, SSR_RESULT_MISSED);
            }
        } else {
            finish(c, SSR_RESULT_MISSED);
        }
        update_players(c, random);
        break;
    }
    case SSR_CHANCE_DRIBBLE:
        dribble(c, touch, random);
        break;
    case SSR_CHANCE_DEFEND:
        defend(c, touch, random);
        break;
    case SSR_CHANCE_OVER: {
        // c_TBall.m_Update: the dead ball moves on until the chance ends. A
        // goal hits the net. A ball that the keeper caught has no power.
        SSRBall *b = &c->ball;
        if (b->power > 0 || b->inGoal) {
            int training = c->info.training;
            int noGoal = is_pass_chance(c->info.chanceType) || c->info.chanceType == 12 || training == 2 || training == 6;
            move_ball(c, noGoal ? NULL : &c->goal, random);
        }
        // The players stop, and the keeper picks up the ball.
        update_players(c, random);
        break;
    }
    }
    c->touchWasDown = touch->down;
}
