#pragma once
#include "ant.hpp"
#include "world.hpp"


struct WorkerUpdater
{
	static void findMarker(Ant& ant, World& world)
	{
		// Init
		SamplingResult result;
		result.max_direction = ant.direction.getVec();
		const Mode marker_phase = ant.getMarkersSamplingType();
		const float sample_angle_range = PI * 0.5f;
		const float current_angle = getAngle(result.max_direction);

		// Sample the world
		const uint32_t sample_count = 32;
		for (uint32_t i(sample_count); i--;) {
			// Get random point in range
			const float sample_angle = current_angle + RNGf::getRange(sample_angle_range);
			const float distance = RNGf::getUnder(ant.marker_detection_max_dist);
			const sf::Vector2f to_marker(cos(sample_angle), sin(sample_angle));
			auto* cell = world.map.getSafe(ant.position + distance * to_marker);
			const HitPoint hit_result = world.map.getFirstHit(ant.position, to_marker, distance);
			// Check cell
			if (!cell || hit_result.cell) {
				if (hit_result.cell) {
					hit_result.cell->discovered = 1.0f;
				}
				continue;
			}
			// Check if
			cell->discovered += 0.1f;
			// Check for food or colony
			if (cell->isPermanent(marker_phase, ant.col_id) || (marker_phase == Mode::ToFood && cell->food)) {
				result.max_direction = to_marker;
				result.found_permanent = true;
				break;
			}
			// Check for enemy
			if (cell->checkEnemyPresence(ant.col_id)) {
				ant.enemy_found = true;
			}
			// Help in case of a fight
			if (cell->checkFight(ant.col_id)) {
				result.found_fight = true;
				result.max_direction = to_marker;
				ant.to_fight_time = 0.0f;
				ant.enemy_found = true;
				break;
			}
			// Flee if repellent
			if (cell->getRepellent(ant.col_id) > result.max_repellent) {
				result.max_repellent = cell->getRepellent(ant.col_id);
				result.repellent_cell = cell;
			}
			// Check for the most intense marker
			const float marker_intensity = to<float>(cell->getIntensity(marker_phase, ant.col_id) * std::pow(cell->wall_dist, 2.0));
			if (marker_intensity > result.max_intensity) {
				result.max_intensity = marker_intensity;
				result.max_direction = to_marker;
				result.max_cell = cell;
			}
			// Eventually choose a different path
			if (RNGf::proba(ant.liberty_coef)) {
				break;
			}
		}
		if (result.found_fight) {
			ant.direction = getAngle(result.max_direction);
			ant.fight_mode = FightMode::ToFight;
			return;
		}
		// Check for repellent
		if (ant.phase == Mode::ToFood && result.max_repellent && !result.found_permanent) {
			const float repellent_prob_factor = 0.3f;
			if (RNGf::proba(repellent_prob_factor * (1.0f - result.max_intensity / to<float>(Conf::MARKER_INTENSITY)))) {
				//phase = Mode::Flee;
				ant.direction.addNow(RNGf::getUnder(2.0f * PI));
				ant.search_markers.reset();
				return;
			}
		}
		// Remove repellent if still food
		if (result.repellent_cell && ant.phase == Mode::ToHome) {
			result.repellent_cell->getRepellent(ant.col_id) *= 0.95f;
		}
		// Update direction
		if (result.max_intensity) {
			if (RNGf::proba(0.2f) && ant.phase == Mode::ToFood) {
				result.max_cell->degrade(ant.col_id, ant.phase, 0.99f);
			}
			ant.direction = getAngle(result.max_direction);
		}
	}

	static void update(Ant& ant, World& world, float dt)
	{
		// Check collision with food
		if (ant.phase == Mode::ToFood) {
			ant.checkFood(world);
		}
		// Get current cell
		WorldCell& cell = world.map.get(ant.position);
		cell.addPresence();
		ant.search_markers.update(dt);
		ant.direction_update.update(dt);
		if (ant.direction_update.ready()) {
			ant.direction_update.reset();
			if (ant.search_markers.ready()) {
				findMarker(ant, world);
				ant.direction += RNGf::getFullRange(ant.direction_noise_range);
			}
			else {
				cell.degrade(ant.col_id, Mode::ToFood, 0.25f);
				ant.direction += RNGf::getFullRange(2.0f * ant.direction_noise_range);
			}
		}
		// Add marker
		ant.marker_add.update(dt);
		if (ant.marker_add.ready()) {
			ant.addMarker(world);
			ant.marker_add.reset();
		}
	}
};
