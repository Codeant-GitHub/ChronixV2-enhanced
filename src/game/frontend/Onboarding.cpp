#pragma once
#include "common.hpp"

#include "Onboarding.hpp"
#include "GUI.hpp"
#include "core/commands/Commands.hpp"
#include "core/commands/BoolCommand.hpp"
#include "game/backend/AnticheatBypass.hpp"
#include "game/pointers/Pointers.hpp"
#include <shellapi.h>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace YimMenu
{
	static BoolCommand _OnboardingComplete{"$onboardingcomplete", "", ""};
	static std::string secret = "c4M8QzNvR71sLKX9e5GhY0TpFd3BJZ2D";

	static size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
	{
		size_t total_size = size * nmemb;
		auto* output = static_cast<std::string*>(userp);
		output->append(static_cast<char*>(contents), total_size);
		return total_size;
	}

	void ProcessOnboarding()
	{
		if (_OnboardingComplete.GetState())
			return;

		static bool ensure_popup_open = [] {
			ImGui::OpenPopup("Welcome to ChronixV2");
			return true;
		}();

		const ImVec2 window_size = {700, 560};
		const ImVec2 window_position = {(*Pointers.ScreenResX - window_size.x) / 2, (*Pointers.ScreenResY - window_size.y) / 2};

		ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);
		ImGui::SetNextWindowPos(window_position, ImGuiCond_Always);

		static std::string user_key, error_message, success_message;
		static bool key_valid = false;

		// Load validation.json from %APPDATA%\ChronixV2
		static const std::filesystem::path validation_path = std::filesystem::path(std::getenv("appdata")) / "ChronixV2" / "validation.json";
		static bool loaded_once = false;

		if (!loaded_once)
		{
			try
			{
				if (std::filesystem::exists(validation_path))
				{
					std::ifstream file(validation_path);
					nlohmann::json j;
					file >> j;
					if (j.contains("userKey"))
						user_key = j["userKey"].get<std::string>();
				}
				else
				{
					std::filesystem::create_directories(validation_path.parent_path());
					std::ofstream file(validation_path);
					file << R"({"userKey": ""})";
				}
			}
			catch (...) {}
			loaded_once = true;
		}

		char key_buffer[256] = {};
		std::strncpy(key_buffer, user_key.c_str(), sizeof(key_buffer) - 1);

		static int sessionChoice = 0;

		if (ImGui::BeginPopupModal("Welcome to ChronixV2", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Welcome to ChronixV2!");
			ImGui::Spacing();
			ImGui::TextWrapped("ChronixV2 brings powerful modding tools and features to your game. Before you begin, please take a moment to understand some important notes about BattlEye and multiplayer behavior.");
			ImGui::Spacing();

			ImGui::SeparatorText("Important Notice About BattlEye");
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
			ImGui::TextWrapped("Due to BattlEye's protections, joining regular public sessions may result in temporary blacklisting (up to 48 hours). You may also be kicked within 3 minutes.");
			ImGui::PopStyleColor();
			ImGui::Spacing();

			ImGui::SeparatorText("Session Mode Configuration");
			ImGui::RadioButton("Join only ChronixV2 users", &sessionChoice, 0);
			ImGui::SameLine();
			ImGui::RadioButton("Join public BattlEye sessions (Not Recommended)", &sessionChoice, 1);

			ImGui::Spacing();
			ImGui::TextWrapped("You can change this later in the menu via:");
			ImGui::BulletText("Network > Spoofing > Join Modder-only Sessions");

			ImGui::Spacing();
			ImGui::SeparatorText("Useful Links");
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);

			if (ImGui::Button("Open ChronixV2 GitHub", ImVec2(250, 0)))
				ShellExecuteA(NULL, "open", "https://github.com/DeadlineEm/ChronixV2", NULL, NULL, SW_SHOWNORMAL);

			ImGui::SameLine();

			if (ImGui::Button("Join Discord Server", ImVec2(250, 0)))
				ShellExecuteA(NULL, "open", "https://discord.gg/ZcC3NP3sAT", NULL, NULL, SW_SHOWNORMAL);

			ImGui::Spacing();
			ImGui::SeparatorText("Menu Key Activation");

			if (ImGui::InputText("Key", key_buffer, sizeof(key_buffer), ImGuiInputTextFlags_CharsNoBlank))
				user_key = key_buffer;

			ImGui::SameLine();
			if (ImGui::Button("Validate Key"))
			{
				CURL* curl = curl_easy_init();
				if (curl)
				{
					std::string url = "https://yeetmodz.net/menuKeys.php?userKey=" + user_key + "&api_key=" + secret;
					std::string response;

					curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
					curl_easy_setopt(curl, CURLOPT_USERAGENT, "ChronixV2 Onboarding Client/1.0");
					curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
					curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
					curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
					curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

					CURLcode res = curl_easy_perform(curl);
					if (res == CURLE_OK)
					{
						try
						{
							auto json_response = nlohmann::json::parse(response);
							if (json_response.contains("status") &&
								json_response["status"] == "success" &&
								json_response.contains("data") &&
								json_response["data"].is_array())
							{
								auto keys = json_response["data"];
								auto it = std::find_if(keys.begin(), keys.end(), [&](const nlohmann::json& key) {
									return key.contains("userKey") && key["userKey"] == user_key;
								});

								if (it != keys.end() && it->contains("activeState") && (*it)["activeState"] == 0)
								{
									success_message = "Key validated successfully! Press 'Get Started'.";
									error_message.clear();
									key_valid = true;
									GUI::SetOnboarding(true);

									// Save key to validation.json
									try
									{
										std::ofstream file(validation_path);
										file << nlohmann::json{{"userKey", user_key}};
									}
									catch (...) {}
								}
								else
								{
									error_message = "Key is locked or invalid.";
									success_message.clear();
									key_valid = false;
									GUI::SetOnboarding(false);
								}
							}
							else
							{
								error_message = "Invalid response from server.";
								key_valid = false;
								GUI::SetOnboarding(false);
							}
						}
						catch (const std::exception& e)
						{
							error_message = std::string("Failed to parse JSON: ") + e.what() + "\nResponse was:\n" + response;
							key_valid = false;
							GUI::SetOnboarding(false);
						}
					}
					else
					{
						error_message = std::string("Connection failed: ") + curl_easy_strerror(res);
						key_valid = false;
						GUI::SetOnboarding(false);
					}
					curl_easy_cleanup(curl);
				}
			}

			if (!error_message.empty())
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", error_message.c_str());

			if (!success_message.empty())
				ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", success_message.c_str());

			ImGui::Spacing();
			ImGui::SeparatorText("Final Tip");
			ImGui::TextWrapped("We release nightly updates. Check for updates regularly for the latest features, improvements, and fixes.");
			ImGui::Spacing();

			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 120) / 2);
			if (ImGui::Button("Get Started", ImVec2(120, 0)) && key_valid)
			{
				// Set cheaterpool based on sessionChoice
				Commands::GetCommand<BoolCommand>("cheaterpool"_J)->SetState(!sessionChoice);
				_OnboardingComplete.SetState(true);
				GUI::SetOnboarding(false);
				ImGui::CloseCurrentPopup();

				// Activate key on backend
				CURL* curl = curl_easy_init();
				if (curl)
				{
					std::string activate_url = "https://yeetmodz.net/menuKeys.php?userKey=" + user_key + "&activeState=1&api_key=" + secret;
					std::string dummy;
					curl_easy_setopt(curl, CURLOPT_URL, activate_url.c_str());
					curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
					curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dummy);
					curl_easy_perform(curl);
					curl_easy_cleanup(curl);
				}
			}
			ImGui::EndPopup();
		}
	}
}
