#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <unordered_map>

#include "AsstDef.h"
#include "AsstAux.h"

#include <json.h>

namespace cv
{
	class Mat;
}
namespace json
{
	class value;
}

namespace asst {
	class WinMacro;
	class Identify;
	class Configer;
	class RecruitConfiger;

	enum TaskType {
		TaskTypeInvalid = 0,
		TaskTypeWindowZoom = 1,
		TaskTypeRecognition = 2,
		TaskTypeClick = 4
	};

	enum class AsstMsg {
		/* Error Msg */
		PtrIsNull,
		ImageIsEmpty,
		WindowMinimized,
		InitFaild,
		/* Info Msg */
		TaskStart,
		ImageFindResult,
		ImageMatched,
		TaskMatched,
		ReachedLimit,
		ReadyToSleep,
		EndOfSleep,
		AppendProcessTask,	// 这个消息Assistance会新增任务，外部不需要处理
		AppendTask,			// 这个消息Assistance会新增任务，外部不需要处理
		TaskCompleted,
		PrintWindow,
		TaskStop,
		TaskError,
		/* Open Recruit Msg */
		TextDetected,
		RecruitTagsDetected,
		OcrResultError,
		RecruitSpecialTag,
		RecruitResult,
		/* Infrast Msg*/
		InfrastOpers,		// 识别到基建干员s
		InfrastComb			// 当前设置的最优干员组合
	};

	// 设施类型
	enum class FacilityType {
		Invalid,
		Manufacturing,		// 制造站
		Trade,				// 贸易站
		PowerStation,		// 发电站
		ControlInterface,	// 控制中枢
		ReceptionRoom,		// 会客室
		Dormitory,			// 宿舍
		Office				// 办公室
		// 训练室和加工站用不上，不加了
	};

	static std::ostream& operator<<(std::ostream& os, const AsstMsg& type)
	{
		static const std::unordered_map<AsstMsg, std::string> _type_name = {
			{AsstMsg::PtrIsNull, "PtrIsNull"},
			{AsstMsg::ImageIsEmpty, "ImageIsEmpty"},
			{AsstMsg::WindowMinimized, "WindowMinimized"},
			{AsstMsg::InitFaild, "InitFaild"},
			{AsstMsg::TaskStart, "TaskStart"},
			{AsstMsg::ImageFindResult, "ImageFindResult"},
			{AsstMsg::ImageMatched, "ImageMatched"},
			{AsstMsg::TaskMatched, "TaskMatched"},
			{AsstMsg::ReachedLimit, "ReachedLimit"},
			{AsstMsg::ReadyToSleep, "ReadyToSleep"},
			{AsstMsg::EndOfSleep, "EndOfSleep"},
			{AsstMsg::AppendProcessTask, "AppendProcessTask"},
			{AsstMsg::TaskCompleted, "TaskCompleted"},
			{AsstMsg::PrintWindow, "PrintWindow"},
			{AsstMsg::TaskError, "TaskError"},
			{AsstMsg::TaskStop, "TaskStop"},
			{AsstMsg::TextDetected, "TextDetected"},
			{AsstMsg::RecruitTagsDetected, "RecruitTagsDetected"},
			{AsstMsg::OcrResultError, "OcrResultError"},
			{AsstMsg::RecruitSpecialTag, "RecruitSpecialTag"},
			{AsstMsg::RecruitResult, "RecruitResult"},
			{AsstMsg::AppendTask, "AppendTask"},
			{AsstMsg::InfrastOpers, "InfrastOpers"},
			{AsstMsg::InfrastComb, "InfrastComb"}

		};
		return os << _type_name.at(type);
	}

	// AsstCallback 消息回调函数
	// 参数：
	// AsstMsg 消息类型
	// const json::value& 消息详情json，每种消息不同，Todo，需要补充个协议文档啥的
	// void* 外部调用者自定义参数，每次回调会带出去，建议传个(void*)this指针进来
	using AsstCallback = std::function<void(AsstMsg, const json::value&, void*)>;

	class AbstractTask
	{
	public:
		AbstractTask(AsstCallback callback, void* callback_arg);
		virtual ~AbstractTask() = default;
		AbstractTask(const AbstractTask&) = default;
		AbstractTask(AbstractTask&&) = default;

		virtual void set_ptr(
			std::shared_ptr<WinMacro> window_ptr,
			std::shared_ptr<WinMacro> view_ptr,
			std::shared_ptr<WinMacro> control_ptr,
			std::shared_ptr<Identify> identify_ptr);
		virtual bool run() = 0;

		virtual void set_exit_flag(bool* exit_flag);
		virtual int get_task_type() { return m_task_type; }
		virtual void set_retry_times(int times) { m_retry_times = times; }
		virtual int get_retry_times() { return m_retry_times; }
		virtual void set_task_chain(std::string name) { m_task_chain = std::move(name); }
		virtual const std::string& get_task_chain() { return m_task_chain; }
	protected:
		virtual cv::Mat get_format_image(bool hd = false);	// 参数hd：高清截图，会慢一点
		virtual bool set_control_scale(double scale);
		virtual bool set_control_scale(int cur_width, int cur_height);
		virtual bool sleep(unsigned millisecond);
		virtual bool print_window(const std::string& dir);

		std::shared_ptr<WinMacro> m_window_ptr = nullptr;
		std::shared_ptr<WinMacro> m_view_ptr = nullptr;
		std::shared_ptr<WinMacro> m_control_ptr = nullptr;
		std::shared_ptr<Identify> m_identify_ptr = nullptr;

		AsstCallback m_callback;
		void* m_callback_arg = NULL;
		bool* m_exit_flag = NULL;
		std::string m_task_chain;
		int m_task_type = TaskType::TaskTypeInvalid;
		int m_retry_times = INT_MAX;
	};

	class ClickTask : public AbstractTask
	{
	public:
		ClickTask(AsstCallback callback, void* callback_arg);
		virtual ~ClickTask() = default;

		virtual bool run() override;
		void set_rect(asst::Rect rect) { m_rect = std::move(rect); };
	protected:
		asst::Rect m_rect;
	};

	class OcrAbstractTask : public AbstractTask
	{
	public:
		OcrAbstractTask(AsstCallback callback, void* callback_arg);
		virtual ~OcrAbstractTask() = default;

		virtual bool run() override = 0;

	protected:
		std::vector<TextArea> ocr_detect();
		std::vector<TextArea> ocr_detect(const cv::Mat& image);

		// 文字匹配，要求相等
		template<typename FilterArray, typename ReplaceMap>
		std::vector<TextArea> text_match(const std::vector<TextArea>& src,
			const FilterArray& filter_array, const ReplaceMap& replace_map)
		{
			std::vector<TextArea> dst;
			for (const TextArea& text_area : src) {
				TextArea temp = text_area;
				for (const auto& [old_str, new_str] : replace_map) {
					temp.text = StringReplaceAll(temp.text, old_str, new_str);
				}
				for (const auto& text : filter_array) {
					if (temp.text == text) {
						dst.emplace_back(std::move(temp));
					}
				}
			}
			return dst;
		}

		// 文字搜索，是子串即可
		template<typename FilterArray, typename ReplaceMap>
		std::vector<TextArea> text_search(const std::vector<TextArea>& src,
			const FilterArray& filter_array, const ReplaceMap& replace_map)
		{
			std::vector<TextArea> dst;
			for (const TextArea& text_area : src) {
				TextArea temp = text_area;
				for (const auto& [old_str, new_str] : replace_map) {
					temp.text = StringReplaceAll(temp.text, old_str, new_str);
				}
				for (const auto& text : filter_array) {
					if (temp.text.find(text) != std::string::npos) {
						dst.emplace_back(text, std::move(temp.rect));
					}
				}
			}
			return dst;
		}
	};

	// 流程任务，按照配置文件里的设置的流程运行
	class ProcessTask : public OcrAbstractTask
	{
	public:
		ProcessTask(AsstCallback callback, void* callback_arg);
		virtual ~ProcessTask() = default;

		virtual bool run() override;

		virtual void set_tasks(const std::vector<std::string>& cur_tasks_name) {
			m_cur_tasks_name = cur_tasks_name;
		}

	protected:
		std::shared_ptr<TaskInfo> match_image(asst::Rect* matched_rect = NULL);
		void exec_click_task(const asst::Rect& matched_rect);

		std::vector<std::string> m_cur_tasks_name;
	};

	class OpenRecruitTask : public OcrAbstractTask
	{
	public:
		OpenRecruitTask(AsstCallback callback, void* callback_arg);
		virtual ~OpenRecruitTask() = default;

		virtual bool run() override;
		virtual void set_param(std::vector<int> required_level, bool set_time = true);

	protected:
		std::vector<int> m_required_level;
		bool m_set_time = false;
	};

	// 基建进驻任务
	class InfrastStationTask : public OcrAbstractTask 
	{
	public:
		InfrastStationTask(AsstCallback callback, void* callback_arg);
		virtual ~InfrastStationTask() = default;

		virtual bool run() override;
		void set_swipe_param(int delay, const Rect& begin_rect, const Rect& end_rect, int max_times = 20)
		{
			m_swipe_delay = delay;
			m_swipe_begin = begin_rect;
			m_swipe_end = end_rect;
			m_swipe_max_times = max_times;
		}
		void set_facility(FacilityType facility)
		{
			m_facility = facility;
		}
	private:
		FacilityType m_facility = FacilityType::Invalid;
		int m_swipe_delay = 0;
		Rect m_swipe_begin;
		Rect m_swipe_end;
		int m_swipe_max_times = 0;
	};

	// for debug
	class TestOcrTask : public OcrAbstractTask
	{
	public:
		TestOcrTask(AsstCallback callback, void* callback_arg);
		virtual ~TestOcrTask() = default;

		virtual bool run() override;
		void set_text(std::string text, bool need_click = false)
		{
			m_text_vec.clear();
			m_text_vec.emplace_back(std::move(text));
			m_need_click = need_click;
		}
		void set_text(std::vector<std::string> text_vec, bool need_click = false)
		{
			m_text_vec = std::move(text_vec);
			m_need_click = need_click;
		}
	private:
		std::vector<std::string> m_text_vec;
		bool m_need_click = false;
	};
}
