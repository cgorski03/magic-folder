use notify_debouncer_full::{new_debouncer, DebounceEventResult, DebouncedEvent, Debouncer};
use notify::{RecommendedWatcher, RecursiveMode};
use std::path::PathBuf;
use std::sync::mpsc::{Receiver, RecvError};
use std::time::Duration;

pub struct FileWatcher {
    _debouncer: Debouncer<RecommendedWatcher>,  // Keep alive with _prefix
    receiver: Receiver<Result<Vec<DebouncedEvent>, Vec<notify::Error>>>,
}

impl FileWatcher {
    pub fn new(path: PathBuf) -> anyhow::Result<Self> {
        let (tx, rx) = std::sync::mpsc::channel();
        let mut debouncer = new_debouncer(Duration::from_secs(2), None, tx)?;

        debouncer
            .watcher()
            .watch(&path, RecursiveMode::Recursive)?;

        Ok(Self {
            _debouncer: debouncer,
            receiver: rx,
        })
    }

    pub fn next_event(&self) -> Result<Vec<DebouncedEvent>, RecvError> {
        match self.receiver.recv()? {
            Ok(events) => Ok(events),
            Err(errors) => {
                eprintln!("Watcher errors: {:?}", errors);
                Ok(vec![])
            }
        }
    }

    pub fn try_next_event(&self) -> Result<Option<Vec<DebouncedEvent>>, RecvError> {
        match self.receiver.try_recv() {
            Ok(Ok(events)) => Ok(Some(events)),
            Ok(Err(errors)) => {
                eprintln!("Watcher errors: {:?}", errors);
                Ok(None)
            }
            Err(std::sync::mpsc::TryRecvError::Empty) => Ok(None),
            Err(std::sync::mpsc::TryRecvError::Disconnected) => Err(RecvError),
        }
    }
}
