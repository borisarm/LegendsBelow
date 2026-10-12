// Events.ixx - EventBus: typed events published by one part and handled by others, queued until dispatched
export module LB.Core:Events;

import std;

export namespace LB {

    /// <summary>
    /// Typed publish and subscribe between parts that should not call each other (the simulation and
    /// what presents it, a service and the systems): Publish queues an event, Dispatch hands each
    /// queued event, in publish order, to the handlers subscribed to its type. Events published
    /// while dispatching are delivered in the same Dispatch, after those already queued.
    /// </summary>
    class EventBus {
    public:
        template<typename Event>
        using Handler = std::function<void(const Event&)>;

        /// <summary>
        /// Handle every event of a type from now on (handlers run in subscription order)
        /// </summary>
        template<typename Event>
        void Subscribe(Handler<Event> handler) {
            handlers_[std::type_index(typeid(Event))].push_back(
                [handler = std::move(handler)](const void* event) { handler(*static_cast<const Event*>(event)); });
        }

        /// <summary>
        /// Queue an event for the next Dispatch
        /// </summary>
        template<typename Event>
        void Publish(Event event) {
            queue_.push_back({ std::type_index(typeid(Event)), std::make_shared<const Event>(std::move(event)) });
        }

        /// <summary>
        /// Deliver the queued events (and those their handlers publish, up to MaxDeliveries, which
        /// stops a cycle of events publishing each other); returns how many were delivered
        /// </summary>
        std::size_t Dispatch() {
            std::size_t delivered = 0;
            for (std::size_t i = 0; i < queue_.size() && delivered < MaxDeliveries; ++i) {
                const auto [type, payload] = queue_[i];   // a copy: handlers may publish (and grow the queue)
                ++delivered;
                const auto handlers = handlers_.find(type);
                if (handlers == handlers_.end()) continue;
                for (const auto& handler : handlers->second) handler(payload.get());
            }
            queue_.clear();
            return delivered;
        }

        [[nodiscard]] std::size_t Pending() const noexcept { return queue_.size(); }

        /// <summary>
        /// Drop the queued events (the handlers stay)
        /// </summary>
        void Clear() noexcept { queue_.clear(); }

        static constexpr std::size_t MaxDeliveries = 10000;

    private:
        struct Queued {
            std::type_index type;
            std::shared_ptr<const void> payload;
        };

        std::unordered_map<std::type_index, std::vector<std::function<void(const void*)>>> handlers_;
        std::vector<Queued> queue_;
    };

} // namespace LB
