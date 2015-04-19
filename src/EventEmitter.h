#ifndef WEBRTCEVENTEMITTER_H
#define WEBRTCEVENTEMITTER_H

#include <queue>
#include <string>
#include <uv.h>

namespace WebRTC { 
  template<class T> class EventWrapper;
  
  class Event {
    template<class T> friend class EventWrapper;
    friend class EventEmitter;
    
   public:
    inline bool HasWrap() const {
      return _wrap;
    }
    
    template <class T> inline T Type() const { 
      return static_cast<T>(_event);
    }
    
    template<class T> const T &Unwrap() const {
      static T nowrap;
      
      if (_wrap) {
        const EventWrapper<T> *ptr = static_cast<const EventWrapper<T> *>(this);
        return ptr->_content;
      }
      
      nowrap = T();
      return nowrap;
    }
   
   private: 
    explicit Event(int event = 0) :
      _event(event),
      _wrap(false)
    { }
    
    virtual ~Event() { }
    
   protected:
    int _event;
    bool _wrap;
  };
  
  template<class T> class EventWrapper : public Event {
    friend class Event;
    friend class EventEmitter;

   private:
    explicit EventWrapper(int event, const T &content) :
      Event(event),
      _content(content)
    {
      _wrap = true;
    }

    virtual ~EventWrapper() { }

   protected:
    T _content;
  };  

  class EventEmitter {    
   public:
    EventEmitter() :
      _running(false),
      _closing(false)
    {
      uv_mutex_init(&_lock);
      _async.data = this;
    }
    
    virtual ~EventEmitter() {
      EventEmitter::End();
      
      while (!_events.empty()) {
        Event *event = _events.front();
        _events.pop();
        delete event;
      }
      
      uv_mutex_destroy(&_lock); 
    }
    
    inline void Start() {
      uv_mutex_lock(&_lock);

      if (!_running) {
        uv_async_init(uv_default_loop(), &_async, reinterpret_cast<uv_async_cb>(EventEmitter::onAsync));
        _running = true;
      }

      uv_mutex_unlock(&_lock);
    }
    
    inline void Stop() {
      uv_mutex_lock(&_lock);

      if (_running) {
        if (_events.empty()) {
          uv_close((uv_handle_t*) &_async, NULL);
          _running = false;
        } else {
          _closing = true;
          uv_async_send(&_async);
        }
      }

      uv_mutex_unlock(&_lock);
    }
    
    inline void End() {
      uv_mutex_lock(&_lock);

      if (_running) {
        uv_close((uv_handle_t*) &_async, NULL);
        _running = false;
      }

      uv_mutex_unlock(&_lock);
    }
    
    inline void Emit(Event *event) {
      uv_mutex_lock(&_lock);

      if (_running) {
        if (!_closing) {
          _events.push(event);
        }
        
        uv_async_send(&_async);
      }

      uv_mutex_unlock(&_lock);
    }
    
    inline void Emit(int event = 0) {
      EventEmitter::Emit(new Event(event));
    }
    
    template <class T> inline void Emit(int event, const T &content) {
      EventWrapper<T> *wrap = new EventWrapper<T>(event, content);
      EventEmitter::Emit(wrap);
    }
    
    virtual void On(Event *event) = 0;
    
   private:
    inline static void onAsync(uv_async_t *handle, int status) {
      EventEmitter *self = static_cast<EventEmitter*>(handle->data);
      bool closing = false;
      uv_mutex_lock(&self->_lock);

      while (!self->_events.empty() && self->_running) {
        Event *event = self->_events.front();
        self->_events.pop();
        
        uv_mutex_unlock(&self->_lock);
        
        self->On(event);
        delete event;
        
        uv_mutex_lock(&self->_lock);
      }

      closing = self->_closing;
      uv_mutex_unlock(&self->_lock);

      if (closing) {
        self->Stop();
      }
    }
    
   protected:
    bool _running;
    bool _closing;
    uv_mutex_t _lock;
    uv_async_t _async;
    std::queue<Event*> _events;
  };
};

#endif